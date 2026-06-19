/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*       (c) 2003 - 2022  SEGGER Microcontroller GmbH                 *
*                                                                    *
*       www.segger.com     Support: support_emfile@segger.com        *
*                                                                    *
**********************************************************************
*                                                                    *
*       emFile * File system for embedded applications               *
*                                                                    *
*                                                                    *
*       Please note:                                                 *
*                                                                    *
*       Knowledge of this file may under no circumstances            *
*       be used to write a similar product for in-house use.         *
*                                                                    *
*       Thank you for your fairness !                                *
*                                                                    *
**********************************************************************
*                                                                    *
*       emFile version: V5.18.1                                      *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
Licensing information
Licensor:                 SEGGER Microcontroller Systems LLC
Licensed to:              Cardinal Detecto, 102 East Daugherty St, Webb City, MO 64870
Licensed SEGGER software: emFile
License number:           FS-00842
License model:            SSL [Single Developer Single Platform Source Code License]
Licensed product:         -
Licensed platform:        Xtensa LX6 (ESP32), Eclipse
Licensed number of seats: 1
----------------------------------------------------------------------
Support and Update Agreement (SUA)
SUA period:               2022-03-24 - 2022-09-24
Contact to extend SUA:    sales@segger.com
----------------------------------------------------------------------
File        : FS_Partition.c
Purpose     : Volume partition tools
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS_ConfDefaults.h"        // File system configuration
#include "FS_Int.h"

/*********************************************************************
*
*       ASSERT_PART_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex)                                            \
    if ((PartIndex) >= (unsigned)FS_NUM_PARTITIONS) {                                         \
      FS_DEBUG_ERROROUT((FS_MTYPE_API, "PART_API: Invalid partition index: %d.", PartIndex)); \
    }
#else
  #define ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex)
#endif

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/
typedef struct {
  U32 NumSectors;
  U8  NumHeads;
  U8  SectorsPerTrack;
} CHS_INFO;

/*********************************************************************
*
*       Static const data
*
**********************************************************************
*/
static const CHS_INFO _aCHSInfo[] = {
  {0x0000FFFuL,   2,   16},  // Up to     2 MBytes
  {0x0007FFFuL,   2,   32},  // Up to    16 MBytes
  {0x000FFFFuL,   4,   32},  // Up to    32 MBytes
  {0x003FFFFuL,   8,   32},  // Up to   128 MBytes
  {0x007FFFFuL,  16,   32},  // Up to   256 MBytes
  {0x00FBFFFuL,  16,   63},  // Up to   504 MBytes
  {0x01F7FFFuL,  32,   63},  // Up to  1008 MBytes
  {0x03EFFFFuL,  64,   63},  // Up to  2016 MBytes
  {0x07DFFFFuL, 128,   63},  // Up to  4032 MBytes
  {0x07DFFFFuL, 255,   63},  // Up to 32768 MBytes
};

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _LoadNumSectors
*
*  Function description
*    Returns the number of sectors of the specified partition
*
*  Parameters
*    PartIndex    The partition index. Valid range is 0..3.
*                 Since this is an internal function, this parameter is not checked for validity.
*    pBuffer      [IN]  Buffer storing the MBR.
*
*  Return value
*    <  0xFFFFFFFF    Number of sectors.
*    == 0xFFFFFFFF    Invalid partition specified.
*/
static U32 _LoadNumSectors(U8 PartIndex, const U8 * pBuffer) {
  unsigned Off;

  Off  = MBR_OFF_PARTITION0 + ((unsigned)PartIndex * PART_ENTRY_SIZE);
  Off += PART_ENTRY_OFF_NUM_SECTORS;
  return FS_LoadU32LE(&pBuffer[Off]);
}

/*********************************************************************
*
*       _LoadStartSector
*
*  Function description
*    Returns the index of the start sector of the specified partition.
*
*  Parameters
*    PartIndex    The partition index. Valid range is 0..3.
*                 Since this is an internal function, this parameter is not checked for validity.
*    pBuffer      [IN]  Buffer storing the MBR.
*
*  Return value
*    <  0xFFFFFFFF    The value of the start sector.
*    == 0xFFFFFFFF    Invalid partition specified.
*/
static U32 _LoadStartSector(U8 PartIndex, const U8 * pBuffer) {
  unsigned Off;

  Off  = MBR_OFF_PARTITION0 + ((unsigned)PartIndex * PART_ENTRY_SIZE);
  Off += PART_ENTRY_OFF_START_SECTOR;
  return FS_LoadU32LE(&pBuffer[Off]);
}

/*********************************************************************
*
*       _HasSignature
*
*  Function description
*    Verifies if the MBR signature is present.
*/
static U8 _HasSignature(const U8 * pBuffer) {
  U16 Data;

  Data = FS_LoadU16LE(pBuffer + MBR_OFF_SIGNATURE);
  if (Data == 0xAA55u) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _IsBPB
*
*  Function description
*    Checks if the buffer stores a Boot Parameter Block.
*    This is indicated by an unconditional x86 jmp instruction stored at the beginning of the buffer.
*/
static U8 _IsBPB(const U8 * pBuffer) {
  //
  // Check for the 1-byte relative jump with opcode 0xe9
  //
  if (pBuffer[0] == 0xE9u) {
    return 1;
  }
  //
  // Check for the 2-byte relative jump with opcode 0xeb
  //
  if ((pBuffer[0] == 0xEBu) && (pBuffer[2] == 0x90u)) {
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*       _GetFirstPartitionInfo
*
*  Function description
*    Returns the start sector and the number of sectors in the first partition.
*
*  Parameters
*    pVolume        [IN]  Volume to read from.
*    pNumSectors    [OUT] Number of sectors in the first partition.
*    pBuffer        Buffer to read the contents of the first sector.
*
*  Return value
*    < 0xFFFFFFF    Index of the first sector of the partition.
*    ==0xFFFFFFF    No valid MBR/BPB found.
*/
static U32 _GetFirstPartitionInfo(FS_VOLUME * pVolume, U32 * pNumSectors, U8 * pBuffer) {
  U32         StartSector;
  U32         NumSectors;
  FS_DEVICE * pDevice;
  int         r;
  FS_DEV_INFO DeviceInfo;

  pDevice = &pVolume->Partition.Device;
  r = FS_LB_ReadDevice(pDevice, 0uL, pBuffer, FS_SECTOR_TYPE_DATA);
  if (r != 0) {
    return SECTOR_INDEX_INVALID;      // Error, sector read failed.
  }
  r = FS_LB_GetDeviceInfo(pDevice, &DeviceInfo);
  if (r != 0) {
    return SECTOR_INDEX_INVALID;      // Error, could not get device info.
  }
  StartSector = 0;
  NumSectors  = DeviceInfo.NumSectors;
  if (_HasSignature(pBuffer) != 0u) {
    if (_IsBPB(pBuffer) == 0u) {
      U32 NumSectorsInPart;

      //
      // Seems to not be a valid BPB.
      // We now assume that it is a boot sector which contains a valid partition table.
      //
      StartSector = _LoadStartSector(0, pBuffer);
      NumSectors  = _LoadNumSectors(0, pBuffer);
      if ((NumSectors == 0u) || (StartSector == 0u)) {
        return SECTOR_INDEX_INVALID;    // Error, partition table entry 0 is not valid.
      }
      //
      // Allow a tolerance of 0.4% in order of having a larger partition than are reported by device.
      //
      NumSectorsInPart = ((StartSector + NumSectors) * 255u) >> 8;
      if (NumSectorsInPart > DeviceInfo.NumSectors) {
        FS_DEBUG_WARN((FS_MTYPE_API, "PART_API: Size of first partition (%lu) is larger than the device size (%lu).", NumSectorsInPart, DeviceInfo.NumSectors));
        return SECTOR_INDEX_INVALID;    // Error, partition table entry 0 is out of bounds.
      }
    }
  }
  if (pNumSectors != NULL) {
    *pNumSectors = NumSectors;
  }
  return StartSector;
}

/*********************************************************************
*
*       _LocatePartition
*
*  Function description
*    Static helper for the FS__LocatePartition() function.
*
*  Return value
*    ==0    O.K.
*    !=0    Error code indicating the failure reason.
*/
static int _LocatePartition(FS_VOLUME * pVolume, U8 * pBuffer) {
  U32 StartSector;
  U32 NumSectors;

  NumSectors = 0;
  //
  // Calculate start sector of the first partition
  //
  StartSector = _GetFirstPartitionInfo(pVolume, &NumSectors, pBuffer);
  if (StartSector == SECTOR_INDEX_INVALID) {    // Is MBR/BPB invalid?
    return 0;                                   // Error, invalid MBR/BPB
                                                // TBD: Forward the error occurred in _GetFirstPartitionInfo()
  }
  pVolume->Partition.StartSector = StartSector;
  pVolume->Partition.NumSectors  = NumSectors;
  return 0;
}

/*********************************************************************
*
*       Public code (internal)
*
**********************************************************************
*/

/*********************************************************************
*
*       FS__LocatePartition
*
*  Return value
*    ==0    O.K., partition located.
*    !=0    Error indicating the failure reason.
*/
int FS__LocatePartition(FS_VOLUME * pVolume) {
  int   r;
  U8  * pBuffer;
  U16   BytesPerSector;

  r              = FS_ERRCODE_BUFFER_NOT_AVAILABLE;   // Set to indicate an error.
  BytesPerSector = FS_GetSectorSize(&pVolume->Partition.Device);
  pBuffer        = FS__AllocSectorBuffer();
  if (pBuffer != NULL) {
    //
    // Check if the a sector fits into the sector buffer.
    //
    if ((BytesPerSector > FS_Global.MaxSectorSize) || (BytesPerSector == 0u)) {
      FS_DEBUG_ERROROUT((FS_MTYPE_API, "FS__LocatePartition: Invalid sector size: %d.", BytesPerSector));
      r = FS_ERRCODE_STORAGE_NOT_READY;               // Error, could not get sector size.
    } else {
      r = _LocatePartition(pVolume, pBuffer);
    }
    FS__FreeSectorBuffer(pBuffer);
  }
  return r;
}

/*********************************************************************
*
*       FS__LoadPartitionInfo
*
*  Function description
*    Returns information about a partition. Data is read from the pBuffer which stores the MBR.
*/
void FS__LoadPartitionInfo(U8 PartIndex, FS_PARTITION_INFO * pPartInfo, const U8 * pBuffer) {
  unsigned   Off;
  const U8 * p;
  int        IsActive;
  U8         Status;

  Off    = MBR_OFF_PARTITION0 + ((unsigned)PartIndex * PART_ENTRY_SIZE);
  p      = pBuffer + Off;
  Status = *p++;
  if ((Status & PART_ENTRY_STATUS_ACTIVE) != 0u) {    // Bootable?
    IsActive = 1;
  } else {                                            // Non-bootable?
    IsActive = 0;
  }
  pPartInfo->IsActive            = (U8)IsActive;
  pPartInfo->StartAddr.Head      = (U8)*p++;
  pPartInfo->StartAddr.Sector    = (U8)(*p & 0x3Fu);
  pPartInfo->StartAddr.Cylinder  = (U16)(((unsigned)*p++ & 0xC0u) << 2);
  pPartInfo->StartAddr.Cylinder += (U16)*p++;
  pPartInfo->Type                = (U8)*p++;
  pPartInfo->EndAddr.Head        = (U8)*p++;
  pPartInfo->EndAddr.Sector      = (U8)(*p & 0x3Fu);
  pPartInfo->EndAddr.Cylinder    = (U16)(((unsigned)*p++ & 0xC0u) << 2);
  pPartInfo->EndAddr.Cylinder   += (U16)*p++;
  pPartInfo->StartSector         = FS_LoadU32LE(p);
  p += 4;
  pPartInfo->NumSectors          = FS_LoadU32LE(p);
}

/*********************************************************************
*
*       FS__StorePartitionInfo
*
*  Function description
*    Stores a partition entry to provided buffer.
*/
void FS__StorePartitionInfo(U8 PartIndex, const FS_PARTITION_INFO * pPartInfo, U8 * pBuffer) {
  unsigned   Off;
  U8       * p;
  U8         Status;

  Off  = MBR_OFF_PARTITION0 + ((unsigned)PartIndex * PART_ENTRY_SIZE);
  p    = pBuffer + Off;
  if (pPartInfo->IsActive != 0u) {
    Status = PART_ENTRY_STATUS_ACTIVE;
  } else {
    Status = PART_ENTRY_STATUS_INACTIVE;
  }
  *p++ = Status;
  *p++ = pPartInfo->StartAddr.Head;
  *p++ = (U8)(((unsigned)pPartInfo->StartAddr.Sector & 0x003Fu)
       | (((unsigned)pPartInfo->StartAddr.Cylinder & 0x0300u) >> 2));
  *p++ = (U8)(pPartInfo->StartAddr.Cylinder & 0x00FFu);
  *p++ = pPartInfo->Type;
  *p++ = pPartInfo->EndAddr.Head;
  *p++ = (U8)(((unsigned)pPartInfo->EndAddr.Sector & 0x003Fu)
       | (((unsigned)pPartInfo->EndAddr.Cylinder & 0x0300u) >> 2));
  *p++ = (U8)(pPartInfo->EndAddr.Cylinder & 0x00FFu);
  FS_StoreU32LE(p, pPartInfo->StartSector);
  p += 4;
  FS_StoreU32LE(p, pPartInfo->NumSectors);
}

/*********************************************************************
*
*       FS__CalcPartitionInfo
*
*  Function description
*    Computes location of a partition on the storage medium in CHS (Cylinder/Head/Sector)
*    and the type of partition.
*
*  Parameters
*    pPartInfo          [IN]  Partition location in sectors (StartSectorAbs and NumSectors must be specified)
*                       [OUT] CHS location of the partition and the partition type.
*    NumSectorsDevice   Total number of sectors on the storage medium.
*/
void FS__CalcPartitionInfo(FS_PARTITION_INFO * pPartInfo, U32 NumSectorsDevice) {
  U32 PartFirstSector;
  U32 PartLastSector;
  U32 Data;
  U32 NumSectorsInPart;
  U8  PartType;
  unsigned         i;
  const CHS_INFO * pCHSInfo;

  //
  // Get CHS info from the table based on the number of sectors on the storage medium.
  //
  pCHSInfo = _aCHSInfo;
  for (i = 0; i < SEGGER_COUNTOF(_aCHSInfo); i++) {
    pCHSInfo = &_aCHSInfo[i];
    if (pCHSInfo->NumSectors > NumSectorsDevice) {
      break;
    }
  }
  NumSectorsInPart = pPartInfo->NumSectors;
  PartFirstSector  = pPartInfo->StartSector;
  PartLastSector   = PartFirstSector + NumSectorsInPart - 1u;
  //
  // Compute the start of partition.
  //
  Data                           = PartFirstSector % ((U32)pCHSInfo->NumHeads * pCHSInfo->SectorsPerTrack);
  Data                          /= pCHSInfo->SectorsPerTrack;
  pPartInfo->StartAddr.Head      = (U8)Data;
  Data                           = (PartFirstSector % pCHSInfo->SectorsPerTrack) + 1u;
  pPartInfo->StartAddr.Sector    = (U8)Data;
  Data                           = PartFirstSector / ((U32)pCHSInfo->NumHeads * pCHSInfo->SectorsPerTrack);
  pPartInfo->StartAddr.Cylinder  = (U16)Data;
  //
  // Compute the end of partition.
  //
  Data                           = PartLastSector % ((U32)pCHSInfo->NumHeads * pCHSInfo->SectorsPerTrack);
  Data                          /= pCHSInfo->SectorsPerTrack;
  pPartInfo->EndAddr.Head        = (U8)Data;
  Data                           = (PartLastSector % pCHSInfo->SectorsPerTrack) + 1u;
  pPartInfo->EndAddr.Sector      = (U8)Data;
  Data                           = PartLastSector / ((U32)pCHSInfo->NumHeads * pCHSInfo->SectorsPerTrack);
  pPartInfo->EndAddr.Cylinder    = (U16)Data;
  //
  // Determine the partition type.
  //
  if        (NumSectorsInPart < 0x7FA8uL) {
    PartType = 0x01;
  } else if (NumSectorsInPart < 0x010000uL) {
    PartType = 0x04;
  } else if (NumSectorsInPart < 0x400000uL) {
    PartType = 0x06;
  } else if (NumSectorsInPart < 0xFB0400uL) {
    PartType = 0x0B;
  } else {
    PartType = 0x0C;
  }
  pPartInfo->Type = PartType;
}

/*********************************************************************
*
*       FS__CalcDeviceInfo
*
*  Function description
*    Computes the number of sectors per track and the number of heads of a device.
*
*  Parameters
*    pDevInfo     [IN]  Number of sectors on the device.
*                 [OUT] Number of sectors per track and the number of heads.
*/
void FS__CalcDeviceInfo(FS_DEV_INFO * pDevInfo) {
  unsigned         i;
  const CHS_INFO * pCHSInfo;

  pCHSInfo = _aCHSInfo;
  for (i = 0; i < SEGGER_COUNTOF(_aCHSInfo); i++) {
    pCHSInfo = &_aCHSInfo[i];
    if (pCHSInfo->NumSectors > pDevInfo->NumSectors) {
      break;
    }
  }
  pDevInfo->SectorsPerTrack = pCHSInfo->SectorsPerTrack;
  pDevInfo->NumHeads        = pCHSInfo->NumHeads;
}

/*********************************************************************
*
*       FS__WriteMBR
*
*  Function description
*    Writes the Master Boot Record to the specified sector of the storage medium.
*
*  Parameters
*    pVolume        [IN]  MBR is created on this storage device.
*    pPartInfo      [IN]  Partition list.
*    NumPartitions  Number of partitions to create, typ. 4 entries.
*                   Can be 0 in which case the no MBR is created and the sector 0 is filled with 0's.
*
*  Return value
*    ==0    O.K.
*    !=0    Error code indicating the failure reason.
*/
int FS__WriteMBR(FS_VOLUME * pVolume, const FS_PARTITION_INFO * pPartInfo, int NumPartitions) {
  U8        * pBuffer;
  int         iPart;
  int         r;
  U8        * p;
  FS_DEVICE * pDevice;

  r       = FS_ERRCODE_BUFFER_NOT_AVAILABLE;  // Set to indicate an error.
  pDevice = &pVolume->Partition.Device;
  pBuffer = FS__AllocSectorBuffer();
  if (pBuffer != NULL) {
    FS_MEMSET(pBuffer, 0, FS_Global.MaxSectorSize);
    //
    // Store the partition entries.
    //
    for (iPart = 0; iPart < NumPartitions; ++iPart) {
      FS__StorePartitionInfo((U8)iPart, pPartInfo, pBuffer);
      ++pPartInfo;
    }
    //
    // Store the signature. If the number of partitions is 0 the MBR is not created and the signature is not needed.
    //
    if (NumPartitions != 0) {
      p = pBuffer + MBR_OFF_SIGNATURE;
      FS_StoreU16LE(p, MBR_SIGNATURE);
    }
    //
    // Write the MBR sector to storage medium.
    //
    r = FS_LB_WriteDevice(pDevice, 0uL, pBuffer, FS_SECTOR_TYPE_MAN, 0);
    if (r != 0) {
      r = FS_ERRCODE_WRITE_FAILURE;
    }
    FS__FreeSectorBuffer(pBuffer);
  }
  return r;
}

/*********************************************************************
*
*       FS__CreateMBR
*
*  Function description
*    Helper for the FS_CreateMBR() function.
*    Creates a Master Boot Record on the specified sector of the storage medium.
*
*  Parameters
*    pVolume        [IN]  MBR is created on this storage device.
*    pPartInfo      [IN]  Partition list.
*    NumPartitions  Number of partitions to create, typ. 4 entries.
*
*  Return value
*    ==0    O.K., Master Boot Record created.
*    !=0    Error code indicating the failure reason.
*/
int FS__CreateMBR(FS_VOLUME * pVolume, FS_PARTITION_INFO * pPartInfo, int NumPartitions) {
  int r;
  int iPart;
  U32 NumSectorsDevice;
  int Status;
  FS_PARTITION_INFO * pPartInfoIter;
  FS_DEVICE         * pDevice;

  r                = FS_ERRCODE_STORAGE_NOT_READY;  // Set to indicate an error.
  NumSectorsDevice = 0;
  pDevice          = &pVolume->Partition.Device;
  pPartInfoIter    = pPartInfo;
  FS_LOCK_DRIVER(pDevice);
  Status  = FS_LB_GetStatus(pDevice);
  //
  // Create MBR only if the medium is present.
  //
  if (Status != FS_MEDIA_NOT_PRESENT) {
    //
    // For all created partitions fill in the missing parameters.
    //
    for (iPart = 0; iPart < NumPartitions; ++iPart) {
      //
      // If not specified, determine the type of partition and the CHS parameters.
      //
      if (pPartInfoIter->Type == 0u) {
        //
        // Get the number of sectors on the storage medium if required.
        //
        if (NumSectorsDevice == 0u) {
          FS_DEV_INFO DevInfo;

          //
          // Get the number of sectors from the driver.
          //
          r = FS_LB_GetDeviceInfo(pDevice, &DevInfo);
          if (r == 0) {
            NumSectorsDevice = DevInfo.NumSectors;
          }
        }
        FS__CalcPartitionInfo(pPartInfoIter, NumSectorsDevice);
      }
      ++pPartInfoIter;
    }
    //
    // Store the MBR on the device.
    //
    r = FS__WriteMBR(pVolume, pPartInfo, NumPartitions);
  }
  FS_UNLOCK_DRIVER(pDevice);
  return r;
}

/*********************************************************************
*
*       FS__GetPartitionInfo
*
*  Function description
*    Helper for the FS_GetPartitionInfo() function.
*    Returns information about 1 partition.
*
*  Parameters
*    pVolume        [IN]  Information is read from this storage device.
*    pPartInfo      [OUT] Partition information.
*    PartIndex      Index of the partition to query.
*
*  Return value
*    ==0    OK, partition information read.
*    !=0    Error code indicating the failure reason.
*/
int FS__GetPartitionInfo(FS_VOLUME * pVolume, FS_PARTITION_INFO * pPartInfo, U8 PartIndex) {
  U8        * pBuffer;
  int         r;
  FS_DEVICE * pDevice;
  int         Status;

  ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex);
  r       = FS_ERRCODE_STORAGE_NOT_PRESENT; // Set to indicate an error.
  pDevice = &pVolume->Partition.Device;
  FS_LOCK_DRIVER(pDevice);
  Status  = FS_LB_GetStatus(pDevice);
  //
  // Get the info only if the medium is present.
  //
  if (Status != FS_MEDIA_NOT_PRESENT) {
    pBuffer = FS__AllocSectorBuffer();
    if (pBuffer != NULL) {
      FS_MEMSET(pBuffer, 0, FS_Global.MaxSectorSize);
      //
      // Read MBR from storage.
      //
      r = FS_LB_ReadDevice(pDevice, 0uL, pBuffer, FS_SECTOR_TYPE_DATA);
      if (r == 0) {
        //
        // MBR read, check whether the signature is correct.
        //
        if (_HasSignature(pBuffer) == 0u) {
          FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "PART_API: Invalid MBR signature."));
          r = FS_ERRCODE_INVALID_MBR;       // Error, no MBR found.
        } else if (_IsBPB(pBuffer) != 0u) {
          FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "PART_API: No MBR found."));
          r = FS_ERRCODE_INVALID_MBR;       // Error, no MBR found.
        } else {
          //
          // Load partition information from read buffer.
          //
          FS__LoadPartitionInfo(PartIndex, pPartInfo, pBuffer);
          r = FS_ERRCODE_OK;                // OK, partition information read.
        }
      }
      FS__FreeSectorBuffer(pBuffer);
    } else {
      r = FS_ERRCODE_BUFFER_NOT_AVAILABLE;
    }
  }
  FS_UNLOCK_DRIVER(pDevice);
  return r;
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_CreateMBR
*
*  Function description
*    Updates the Master Boot Record (MBR) of a volume.
*
*  Parameters
*    sVolumeName    Volume name for which the MBR has to be updated.
*    pPartInfo      [IN] List of the partitions to be created.
*    NumPartitions  Number of partitions to be create.
*
*  Return value
*    ==0    OK, MBR created.
*    !=0    Error code indicating the failure reason.
*
*  Additional information
*    MBR (Master Boot Record) is a special sector that contains
*    information about how the storage device is partitioned.
*    This information is stored to the first sector of a storage
*    device. The information stored to MBR can be queried via
*    FS_GetPartitionInfo().
*
*    The function overwrites any information stored to the sector
*    index 0 of the specified volume. The partition entries are
*    stored in the order specified in the pPartInfo array:
*    the information from \tt{pPartInfo[0]} is stored to first
*    partition entry, the information from \tt{pPartInfo[1]}
*    is stored to the second one, and so on.
*
*    If the \tt Type  field of the FS_PARTITION_INFO structure
*    is set to 0 the function determines automatically the partition
*    type and the CHS (Cylinder/Head/Sector) addresses (\tt Type,
*    \tt StartAddr and \tt EndAddr) based on the values stored
*    in the \tt StartSector and \tt NumSector members
*    of the same structure.
*
*    The DISKPART logical driver can be used to get access
*    to the created partitions. pVolumeName has to be the name
*    of a volume assigned to a device driver.
*/
int FS_CreateMBR(const char * sVolumeName, FS_PARTITION_INFO * pPartInfo, int NumPartitions) {
  int         r;
  FS_VOLUME * pVolume;

  FS_LOCK();
  pVolume = FS__FindVolume(sVolumeName);
  if (pVolume != NULL) {
    r = FS__CreateMBR(pVolume, pPartInfo, NumPartitions);
  } else {
    r = FS_ERRCODE_VOLUME_NOT_FOUND;        // Error, volume not found.
  }
  FS_UNLOCK();
  return r;
}

/*********************************************************************
*
*       FS_GetPartitionInfo
*
*  Function description
*    Returns information about a MBR partition.
*
*  Parameters
*    sVolumeName    Name of the volume on which the MBR is located.
*    pPartInfo      [OUT] Information about the partition.
*    PartIndex      Index of the partition to query.
*
*  Return value
*    ==0    OK, partition information read.
*    !=0    Error code indicating the failure reason.
*
*  Additional information
*    The function reads the information from the Master Boot Record
*    (MBR) that is stored on the first sector (the sector with the
*    index 0) of the specified volume. An error is returned if no MBR
*    information is present on the volume. If the \tt Type member
*    of the FS_PARTITION_INFO structure is 0, the partition entry
*    is not valid. FS_NUM_PARTITIONS specifies the maximum number
*    of partitions in MBR.
*/
int FS_GetPartitionInfo(const char * sVolumeName, FS_PARTITION_INFO * pPartInfo, U8 PartIndex) {
  int         r;
  FS_VOLUME * pVolume;

  FS_LOCK();
  pVolume = FS__FindVolume(sVolumeName);
  if (pVolume != NULL) {
    r = FS__GetPartitionInfo(pVolume, pPartInfo, PartIndex);
  } else {
    r = FS_ERRCODE_VOLUME_NOT_FOUND;          // Error, the specified volume was not found.
  }
  FS_UNLOCK();
  return r;
}

/*************************** End of file ****************************/

