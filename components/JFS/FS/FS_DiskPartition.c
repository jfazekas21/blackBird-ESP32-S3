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
File        : FS_DiskPartition.c
Purpose     : Logical volume driver
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS_Int.h"

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       ASSERT_UNIT_NO_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_UNIT_NO_IS_IN_RANGE(Unit)                                       \
    if ((Unit) >= (U8)FS_DISKPART_NUM_UNITS) {                                   \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Invalid unit number."));    \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                       \
    }
#else
  #define ASSERT_UNIT_NO_IS_IN_RANGE(Unit)
#endif

/*********************************************************************
*
*       ASSERT_PART_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex)                                \
  if ((PartIndex) >= 4u) {                                                        \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Invalid partition index.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                        \
    }
#else
  #define ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex)
#endif

/*********************************************************************
*
*       ASSERT_SECTORS_ARE_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTORS_ARE_IN_RANGE(pInst, SectorIndex, NumSectors)             \
  if (((SectorIndex) >= (pInst)->NumSectors) ||                                   \
     (((SectorIndex) + (NumSectors)) > (pInst)->NumSectors)) {                    \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Invalid sector index."));    \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                        \
    }
#else
  #define ASSERT_SECTORS_ARE_IN_RANGE(pInst, SectorIndex, NumSectors)
#endif

/*********************************************************************
*
*       ASSERT_DEVICE_IS_SET
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_DEVICE_IS_SET(pInst)                                            \
    if ((pInst)->pDeviceType == NULL) {                                          \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Device not set."));         \
      FS_X_PANIC(FS_ERRCODE_UNKNOWN_DEVICE);                                     \
    }
#else
  #define ASSERT_DEVICE_IS_SET(Unit)
#endif

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/
typedef struct {
  U8                     Unit;
  U8                     DeviceUnit;
  U8                     PartIndex;
  U8                     HasError;
  const FS_DEVICE_TYPE * pDeviceType;
  U32                    StartSector;
  U32                    NumSectors;
  U16                    BytesPerSector;
#if FS_DISKPART_SUPPORT_ERROR_RECOVERY
  FS_READ_ERROR_DATA     ReadErrorData;         // Function to be called when a bit error occurs to get corrected data.
#endif
} DISKPART_INST;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static DISKPART_INST * _apInst[FS_DISKPART_NUM_UNITS];
static U8              _NumUnits = 0;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _InitMedium
*
*  Function description
*    Initializes the underlying driver.
*/
static int _InitMedium(const DISKPART_INST * pInst) {
  int                    r;
  U8                     DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  ASSERT_DEVICE_IS_SET(pInst);
  pDeviceType = pInst->pDeviceType;
  DeviceUnit  = pInst->DeviceUnit;
  r = 0;
  if (pDeviceType->pfInitMedium != NULL) {
    r = pDeviceType->pfInitMedium(DeviceUnit);
    if (r != 0) {
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not initialize the storage device."));
    }
  }
  return r;
}

/*********************************************************************
*
*       _GetDeviceInfo
*
*  Function description
*    Reads device information from the underlying driver.
*/
static int _GetDeviceInfo(const DISKPART_INST * pInst, FS_DEV_INFO * pDevInfo) {
  int                    r;
  U8                     DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  pDeviceType = pInst->pDeviceType;
  DeviceUnit  = pInst->DeviceUnit;
  r = pDeviceType->pfIoCtl(DeviceUnit, FS_CMD_GET_DEVINFO, 0, SEGGER_PTR2PTR(void, pDevInfo));            // MISRA deviation D:100[f]
  if (r != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not get storage info."));
  }
  return r;
}

/*********************************************************************
*
*       _ReadMBR
*
*  Function description
*    Reads the contents of Master Boot Record (MBR) and checks if the signature is valid.
*/
static int _ReadMBR(const DISKPART_INST * pInst, U8 * pBuffer) {
  int                    r;
  U16                    Signature;
  U8                   * p;
  U8                     DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  pDeviceType = pInst->pDeviceType;
  DeviceUnit  = pInst->DeviceUnit;
  //
  // Read the MBR from storage medium. This is always the first sector.
  //
  r = pDeviceType->pfRead(DeviceUnit, 0, pBuffer, 1);
  if (r != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not read MBR."));
    return 1;           // Error, failed to read MBR.
  }
  //
  // MBR read, check whether the signature is correct.
  //
  r = 0;
  p = pBuffer + MBR_OFF_SIGNATURE;
  Signature = FS_LoadU16LE(p);
  if (Signature != MBR_SIGNATURE) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Invalid MBR signature."));
    r = 1;              // Error, invalid signature.
  }
  return r;
}

/*********************************************************************
*
*       _ReadSectors
*
*  Function description
*    Loads a number of sectors from the storage medium.
*/
static int _ReadSectors(const DISKPART_INST * pInst, U32 SectorIndex, void * pBuffer, U32 NumSectors) {
  int                    r;
  U32                    StartSector;
  U8                     DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  if (pInst->HasError != 0u) {
    return 1;
  }
  ASSERT_SECTORS_ARE_IN_RANGE(pInst, SectorIndex, NumSectors);
  ASSERT_DEVICE_IS_SET(pInst);
  pDeviceType  = pInst->pDeviceType;
  DeviceUnit   = pInst->DeviceUnit;
  StartSector  = pInst->StartSector;
  SectorIndex += StartSector;
  r = pDeviceType->pfRead(DeviceUnit, SectorIndex, pBuffer, NumSectors);
  if (r != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not read sectors."));
  }
  return r;
}

/*********************************************************************
*
*       _ReadPartInfo
*
*  Function description
*    Reads information about the location of the partition from MBR.
*/
static int _ReadPartInfo(DISKPART_INST * pInst) {
  int                 r;
  U8                  PartIndex;
  U32                 StartSector;
  U32                 NumSectors;
  U16                 BytesPerSector;
  U8                  HasError;
  U32                 NumSectorsDevice;
  U8                * pBuffer;
  FS_PARTITION_INFO   PartInfo;
  FS_DEV_INFO         DevInfo;

  PartIndex      = pInst->PartIndex;
  StartSector    = 0;
  NumSectors     = 0;
  BytesPerSector = 0;
  HasError       = 1;               // Set to indicate error until the instance is initialized successfully.
  r              = 1;               // Set to indicate error.
  pBuffer = FS__AllocSectorBuffer();
  if (pBuffer != NULL) {
    //
    // Read information form the partition table of MBR.
    //
    r = _ReadMBR(pInst, pBuffer);
    if (r == 0) {
      //
      // The contents of MBR is now in buffer. Decode the configured partition table entry.
      //
      FS__LoadPartitionInfo(PartIndex, &PartInfo, pBuffer);
      if (PartInfo.Type != 0u) {
        StartSector = PartInfo.StartSector;
        NumSectors  = PartInfo.NumSectors;
        //
        // Get the sector size from device.
        //
        r = _GetDeviceInfo(pInst, &DevInfo);
        if (r == 0) {
          BytesPerSector   = DevInfo.BytesPerSector;
          NumSectorsDevice = DevInfo.NumSectors;
          //
          // Validity checks:
          //   - the number of sectors must be valid
          //   - the partition should fit on the storage medium
          //
          if        (NumSectors == 0u) {
            FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Invalid number of sectors."));
          } else if ((StartSector >= NumSectorsDevice) ||
                     ((StartSector + NumSectors) > NumSectorsDevice)) {
            FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Partition exceeds device size."));
          } else {
            FS_DEBUG_LOG((FS_MTYPE_DRIVER, "DISKPART: Found PartIndex: %d, StartSector: %u, NumSectors: %u.\n", PartIndex, StartSector, NumSectors));
            HasError = 0;
          }
        }
      }
    }
    FS__FreeSectorBuffer(pBuffer);
  }
  pInst->HasError       = HasError;
  pInst->StartSector    = StartSector;
  pInst->NumSectors     = NumSectors;
  pInst->BytesPerSector = BytesPerSector;
  return r;
}

/*********************************************************************
*
*       _ReadPartInfoIfRequired
*
*  Function description
*    Reads information about the location of the partition from MBR only if it is not already present.
*/
static int _ReadPartInfoIfRequired(DISKPART_INST * pInst) {
  int r;

  if (pInst->HasError != 0u) {
    return 1;               // Error
  }
  if (pInst->NumSectors != 0u) {
    return 0;               // OK, information already read;
  }
  r = _ReadPartInfo(pInst);
  return r;
}

/*********************************************************************
*
*       _GetStatus
*
*  Function description
*    Returns whether the storage medium is present or not.
*/
static int _GetStatus(const DISKPART_INST * pInst) {
  int                    Status;
  U8                     DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  Status = FS_MEDIA_NOT_PRESENT;
  if ((pInst->HasError == 0u) && (pInst->pDeviceType != NULL)) {
    pDeviceType = pInst->pDeviceType;
    DeviceUnit  = pInst->DeviceUnit;
    Status      = pDeviceType->pfGetStatus(DeviceUnit);
  }
  return Status;
}

/*********************************************************************
*
*       _WriteSectors
*
*  Function description
*    Stores a number of sectors to storage medium.
*/
static int _WriteSectors(const DISKPART_INST * pInst, U32 SectorIndex, const void * pBuffer, U32 NumSectors, U8 RepeatSame) {
  int   r;
  U32   StartSector;
  U8    DeviceUnit;
  const FS_DEVICE_TYPE * pDeviceType;

  if (pInst->HasError != 0u) {
    return 1;
  }
  ASSERT_SECTORS_ARE_IN_RANGE(pInst, SectorIndex, NumSectors);
  ASSERT_DEVICE_IS_SET(pInst);
  pDeviceType  = pInst->pDeviceType;
  DeviceUnit   = pInst->DeviceUnit;
  StartSector  = pInst->StartSector;
  SectorIndex += StartSector;
  r = pDeviceType->pfWrite(DeviceUnit, SectorIndex, pBuffer, NumSectors, RepeatSame);
  if (r != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not write sectors."));
  }
  return r;
}

#if FS_DISKPART_SUPPORT_ERROR_RECOVERY

/*********************************************************************
*
*       _FindInst
*
*  Function description
*    Searches for a driver instance by storage device.
*/
static DISKPART_INST * _FindInst(const FS_DEVICE_TYPE * pDeviceType, U32 DeviceUnit) {
  unsigned        Unit;
  DISKPART_INST * pInst;

  for (Unit = 0; Unit < _NumUnits; ++Unit) {
    pInst = _apInst[Unit];
    if (pInst->pDeviceType == pDeviceType) {
      if (pInst->DeviceUnit == DeviceUnit) {
        return pInst;
      }
    }
  }
  return NULL;       // No matching instance found.
}

/*********************************************************************
*
*       _cbOnReadError
*
*  Function description
*    Function to be called by the driver when a read error occurs.
*
*  Parameters
*    pDeviceType    Type of storage device which encountered the read error.
*    DeviceUnit     Unit number of the storage device where the read error occurred.
*    SectorIndex    Index of the sector where the read error occurred.
*    pBuffer        [OUT] Corrected sector data.
*    NumSectors     Number of sectors on which the read error occurred.
*
*  Return value
*    ==0    Data for the requested sector returned.
*    !=0    An error occurred.
*/
static int _cbOnReadError(const FS_DEVICE_TYPE * pDeviceType, U32 DeviceUnit, U32 SectorIndex, void * pBuffer, U32 NumSectors) {
  DISKPART_INST      * pInst;
  U8                   Unit;
  FS_READ_ERROR_DATA * pReadErrorData;
  int                  r;
  U32                  StartSectorPart;
  U32                  NumSectorsPart;

  pInst = _FindInst(pDeviceType, DeviceUnit);
  if (pInst == NULL) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: _cbOnReadError: No matching instance found (VN: \"%s:%d:\")", pDeviceType->pfGetName((U8)DeviceUnit), (int)DeviceUnit));
    return 1;
  }
  pReadErrorData = &pInst->ReadErrorData;
  if (pReadErrorData->pfCallback == NULL) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: _cbOnReadError: No callback registered."));
    return 1;
  }
  StartSectorPart = pInst->StartSector;
  NumSectorsPart  = pInst->NumSectors;
  if ((SectorIndex < StartSectorPart) || (SectorIndex >= (StartSectorPart + NumSectorsPart))) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: _cbOnReadError: Invalid sector index."));
    return 1;
  }
  Unit         = pInst->Unit;
  SectorIndex -= StartSectorPart;
  r = pReadErrorData->pfCallback(&FS_DISKPART_Driver, Unit, SectorIndex, pBuffer, NumSectors);
  return r;
}

/*********************************************************************
*
*       _SetReadErrorCallback
*
*  Function description
*    Function to be called by the driver when a read error occurs.
*/
static int _SetReadErrorCallback(const DISKPART_INST * pInst) {
  int                    r;
  FS_READ_ERROR_DATA     ReadErrorData;
  const FS_DEVICE_TYPE * pDeviceType;
  U8                     DeviceUnit;

  ASSERT_DEVICE_IS_SET(pInst);
  FS_MEMSET(&ReadErrorData, 0, sizeof(ReadErrorData));
  ReadErrorData.pfCallback = _cbOnReadError;
  pDeviceType = pInst->pDeviceType;
  DeviceUnit  = pInst->DeviceUnit;
  r = pDeviceType->pfIoCtl(DeviceUnit, FS_CMD_SET_READ_ERROR_CALLBACK, 0 /* not used */, &ReadErrorData);
  return r;
}

#endif // FS_DISKPART_SUPPORT_ERROR_RECOVERY

/*********************************************************************
*
*       _GetInst
*/
static DISKPART_INST * _GetInst(U8 Unit) {
  DISKPART_INST * pInst;

  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = NULL;
  if (Unit < (U8)FS_DISKPART_NUM_UNITS) {
    pInst = _apInst[Unit];
  }
  return pInst;
}

/*********************************************************************
*
*       Static code (public via callback)
*
**********************************************************************
*/

/*********************************************************************
*
*       _DISKPART_GetDriverName
*
*   Function description
*     FS driver function. Returns the driver name.
*/
static const char * _DISKPART_GetDriverName(U8 Unit) {
  FS_USE_PARA(Unit);
  return "diskpart";
}

/*********************************************************************
*
*       _DISKPART_AddDevice
*
*   Function description
*     FS driver function. Creates a driver instance.
*/
static int _DISKPART_AddDevice(void) {
  DISKPART_INST * pInst;
  U8              Unit;

  if (_NumUnits >= (U8)FS_DISKPART_NUM_UNITS) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DISKPART: Could not add device. Too many instances."));
    return -1;                      // Error, too many instances defined.
  }
  Unit  = _NumUnits;
  pInst = _apInst[Unit];
  if (pInst == NULL) {
    FS_ALLOC_ZEROED_PTR(SEGGER_PTR2PTR(void*, &pInst), (int)sizeof(DISKPART_INST), "DISKPART_INST");      // MISRA deviation D:100[d]
    if (pInst == NULL) {
      return -1;                    // Error, could not allocate memory.
    }
    _apInst[Unit] = pInst;
    pInst->Unit = Unit;
    ++_NumUnits;
  }
  return (int)Unit;                 // OK, instance created.
}

/*********************************************************************
*
*       _DISKPART_Read
*
*   Function description
*     FS driver function. Reads a number of sectors from storage medium.
*/
static int _DISKPART_Read(U8 Unit, U32 SectorIndex, void * pBuffer, U32 NumSectors) {
  DISKPART_INST * pInst;
  int             r;

  r     = 1;                // Set to indicate an error.
  pInst = _GetInst(Unit);
  if (pInst != NULL) {
    r = _ReadSectors(pInst, SectorIndex, pBuffer, NumSectors);
  }
  return r;
}

/*********************************************************************
*
*       _DISKPART_Write
*
*   Function description
*     FS driver function. Writes a number of sectors to storage medium.
*/
static int _DISKPART_Write(U8 Unit, U32 SectorIndex, const void * pBuffer, U32 NumSectors, U8 RepeatSame) {
  DISKPART_INST * pInst;
  int             r;

  r     = 1;                // Set to indicate an error.
  pInst = _GetInst(Unit);
  if (pInst != NULL) {
    r = _WriteSectors(pInst, SectorIndex, pBuffer, NumSectors, RepeatSame);
  }
  return r;
}

/*********************************************************************
*
*       _DISKPART_IoCtl
*
*   Function description
*     FS driver function. Executes an I/O control command.
*/
static int _DISKPART_IoCtl(U8 Unit, I32 Cmd, I32 Aux, void * pBuffer) {
  DISKPART_INST        * pInst;
  int                    r;
  U8                     DeviceUnit;
  FS_DEV_INFO          * pDevInfo;
  int                    RelayCmd;
  U32                    SectorIndex;
  const FS_DEVICE_TYPE * pDeviceType;

  pInst = _GetInst(Unit);
  if (pInst == NULL) {
    return -1;                    // Instance not allocated
  }
  FS_USE_PARA(Aux);
  r            = -1;              // Set to indicate an error.
  RelayCmd     = 1;               // By default, pass the commands to the underlying driver
  DeviceUnit  = pInst->DeviceUnit;
  pDeviceType = pInst->pDeviceType;
  switch (Cmd) {
  case FS_CMD_GET_DEVINFO:
    if (pBuffer != NULL) {
      r = _ReadPartInfoIfRequired(pInst);
      if (r == 0) {
        pDevInfo = SEGGER_PTR2PTR(FS_DEV_INFO, pBuffer);                                                  // MISRA deviation D:100[f]
        pDevInfo->NumSectors     = pInst->NumSectors;
        pDevInfo->BytesPerSector = pInst->BytesPerSector;
      }
    }
    RelayCmd = 0;                 // Command is handled by this driver.
    break;
#if FS_SUPPORT_DEINIT
  case FS_CMD_DEINIT:
    if (pDeviceType != NULL) {
      r = pDeviceType->pfIoCtl(DeviceUnit, Cmd, Aux, pBuffer);
    }
    RelayCmd = 0;               // Command is handled by this driver.
    FS_FREE(pInst);
    _apInst[Unit] = NULL;
    _NumUnits--;
    break;
#endif
  case FS_CMD_UNMOUNT:
    // through
  case FS_CMD_UNMOUNT_FORCED:
    pInst->HasError       = 0;
    pInst->NumSectors     = 0;
    pInst->StartSector    = 0;
    pInst->BytesPerSector = 0;
    break;
  case FS_CMD_FREE_SECTORS:
    //
    // SectorIndex is relative to the beginning of partition but the driver
    // expects the absolute logical sector index. This command is relayed.
    //
    SectorIndex  = (U32)Aux;
    SectorIndex += pInst->StartSector;
    Aux          = (I32)SectorIndex;
    break;
#if FS_DISKPART_SUPPORT_ERROR_RECOVERY
  case FS_CMD_SET_READ_ERROR_CALLBACK:
    if (pBuffer != NULL) {
      FS_READ_ERROR_DATA * pReadErrorData;

      pReadErrorData       = SEGGER_PTR2PTR(FS_READ_ERROR_DATA, pBuffer);                                 // MISRA deviation D:100[f]
      pInst->ReadErrorData = *pReadErrorData;     // struct copy
      r = _SetReadErrorCallback(pInst);
    }
    RelayCmd = 0;                 // Command is handled by this driver.
    break;
#endif
  default:
    //
    // All other commands are relayed to the underlying driver(s).
    //
    break;
  }
  if (RelayCmd != 0) {
    if (pDeviceType != NULL) {
      r = pDeviceType->pfIoCtl(DeviceUnit, Cmd, Aux, pBuffer);
    }
  }
  return r;
}

/*********************************************************************
*
*       _DISKPART_InitMedium
*
*   Function description
*     FS driver function. Initializes the storage medium.
*/
static int _DISKPART_InitMedium(U8 Unit) {
  int             r;
  DISKPART_INST * pInst;

  r     = 1;                // Set to indicate an error.
  pInst = _GetInst(Unit);
  if (pInst != NULL) {
    r = _InitMedium(pInst);
  }
  return r;
}

/*********************************************************************
*
*       _DISKPART_GetStatus
*
*   Function description
*     FS driver function. Returns whether the storage media is present or not.
*/
static int _DISKPART_GetStatus(U8 Unit) {
  DISKPART_INST * pInst;
  int             Status;

  Status = FS_MEDIA_NOT_PRESENT;    // Set to indicate an error.
  pInst  = _GetInst(Unit);
  if (pInst != NULL) {
    Status = _GetStatus(pInst);
  }
  return Status;
}

/*********************************************************************
*
*       _DISKPART_GetNumUnits
*
*   Function description
*     FS driver function. Returns the number of driver instances.
*/
static int _DISKPART_GetNumUnits(void) {
  return (int)_NumUnits;
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_DISKPART_Driver
*/
const FS_DEVICE_TYPE FS_DISKPART_Driver = {
  _DISKPART_GetDriverName,
  _DISKPART_AddDevice,
  _DISKPART_Read,
  _DISKPART_Write,
  _DISKPART_IoCtl,
  _DISKPART_InitMedium,
  _DISKPART_GetStatus,
  _DISKPART_GetNumUnits
};

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_DISKPART_Configure
*
*  Function description
*    Configures the parameters of a driver instance.
*
*  Parameters
*    Unit           Index of the DISKPART instance to configure.
*    pDeviceType    Type of device driver that is used to access the storage device.
*    DeviceUnit     Index of the device driver instance that is used to access the storage device (0-based).
*    PartIndex      Index of the partition in the partition table stored in MBR.
*
*  Additional information
*    This function has to be called once for each instance of the driver.
*    The application can use FS_DISKPART_Configure() to set the parameters
*    that allows the driver to access the partition table stored in
*    Master Boot Record (MBR). The size and the position of the partition
*    are read from MBR on the first access to storage device.
*/
void FS_DISKPART_Configure(U8 Unit, const FS_DEVICE_TYPE * pDeviceType, U8 DeviceUnit, U8 PartIndex) {
  DISKPART_INST * pInst;

  ASSERT_PART_INDEX_IS_IN_RANGE(PartIndex);
  pInst = _GetInst(Unit);
  if (pInst != NULL) {
    pInst->pDeviceType = pDeviceType;
    pInst->DeviceUnit  = DeviceUnit;
    pInst->PartIndex   = PartIndex;
  }
}

/*************************** End of file ****************************/
