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
-------------------------- END-OF-HEADER -----------------------------

File    : TEMPLATE_Driver.c
Purpose : Template for a FS driver.
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
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_TEMPLATE_NUM_UNITS
  #define FS_TEMPLATE_NUM_UNITS     4     // Maximum number of driver instances.
#endif

/*********************************************************************
*
*       Type definitions
*
**********************************************************************
*/
typedef struct {
  U8  IsInited;
  U8  Unit;
  U16 NumHeads;
  U16 SectorsPerTrack;
  U32 NumSectors;
  U16 BytesPerSector;
} DRIVER_INST;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static DRIVER_INST * _apInst[FS_TEMPLATE_NUM_UNITS];
static int           _NumUnits = 0;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _Init
*
*  Function description
*    Resets/Initializes the device.
*
*  Parameters
*    pInst    Driver instance
*
*  Return value
*    ==0      Device has been reset and initialized.
*    !=0      An error has occurred.
*/
static int _Init(DRIVER_INST * pInst) {
  FS_USE_PARA(pInst);
  return 0;
}

/*********************************************************************
*
*       _Unmount
*
*  Function description
*    Unmounts the volume.
*
*  Parameters
*    pInst    Driver instance
*/
static void _Unmount(DRIVER_INST * pInst) {
  if (pInst->IsInited) {
    FS_MEMSET(pInst, 0, sizeof(DRIVER_INST));
  }
}

/*********************************************************************
*
*       Public code (indirectly via callback)
*
**********************************************************************
*/

/*********************************************************************
*
*       _GetDriverName
*
*  Function description
*    Returns the driver name.
*
*  Parameters
*    Unit   Index of the driver instance (0-based).
*
*  Return value
*    Name of the driver as 0-terminated string.
*
*  Additional information
*    The returned name is used by the file system to create the volume name.
*/
static const char * _GetDriverName(U8 Unit) {
  FS_USE_PARA(Unit);
  return "template";
}

/*********************************************************************
*
*       _AddDevice
*
*  Function description
*    Initializes a new driver instance.
*
*  Return value
*    >=0    OK, index of the created driver instance.
*    < 0    An error occurred.
*/
static int _AddDevice(void) {
  U8            Unit;
  DRIVER_INST * pInst;

  if (_NumUnits >= FS_TEMPLATE_NUM_UNITS) {
    return -1;                                                  // Error, too many driver instances.
  }
  Unit = (U8)_NumUnits++;
  pInst = (DRIVER_INST *)FS_AllocZeroed(sizeof(DRIVER_INST));
  if (pInst == NULL) {
    return -1;                                                  // Error, out of memory.
  }
  _apInst[Unit] = pInst;
  return Unit;
}

/*********************************************************************
*
*       _Read
*
*  Function description
*    Reads one or more logical sectors from storage device.
*
*  Parameters
*    Unit           Index of the driver instance (0-based).
*    SectorIndex    First sector to be read from the device.
*    pData          Pointer to buffer for storing the data.
*    NumSectors     Number of sectors to read.
*
*  Return value
*    ==0    OK, data successfully written.
*    !=0    An error occurred.
*
*  Additional information
*    The sector data has to be stored at the following offsets in pData:
*    Byte offset                        Sector index
*    -----------------------------------------------
*    0                                  SectorIndex
*    BytesPerSector                     SectorIndex + 1
*    BytesPerSector * 2                 SectorIndex + 2
*    BytesPerSector * 3                 SectorIndex + 3
*    ...
*    BytesPerSector * (NumSectors - 1)  SectorIndex + (NumSectors - 1)
*/
static int _Read(U8 Unit, U32 SectorIndex, void * pData, U32 NumSectors) {
  DRIVER_INST * pInst;

  FS_USE_PARA(SectorIndex);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumSectors);
  pInst = _apInst[Unit];
  return pInst->IsInited ? 0 : 1;
}

/*********************************************************************
*
*       _Write
*
*  Function description
*    Writes one or more logical sectors to storage device.
*
*  Parameters
*    Unit         Index of the driver instance (0-based).
*    SectorNo     First sector to be written to the device.
*    pData        Pointer to buffer containing the data to write.
*    NumSectors   Number of sectors to store.
*    RepeatSame   Repeat the same data to all sectors.
*
*  Return value
*    ==0    OK, data successfully written.
*    !=0    An error has occurred.
*
*  Additional information
*    The sector data is stored at the following offsets in pData
*    if RepeatSame is set to 0:
*    Byte offset                        Sector index
*    -----------------------------------------------
*    0                                  SectorIndex
*    BytesPerSector                     SectorIndex + 1
*    BytesPerSector * 2                 SectorIndex + 2
*    BytesPerSector * 3                 SectorIndex + 3
*    ...
*    BytesPerSector * (NumSectors - 1)  SectorIndex + (NumSectors - 1)
*
*    If RepeatSame is set to 1 then pData points to a memory region that
*    is BytePerSector bytes large. This block of data has to be stored
*    repeatedly to the sector indexes SectorIndex to SectorIndex + (NumSectors - 1)
*/
static int _Write(U8 Unit, U32 SectorIndex, const void * pData, U32 NumSectors, U8 RepeatSame) {
  DRIVER_INST * pInst;

  FS_USE_PARA(SectorIndex);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumSectors);
  FS_USE_PARA(RepeatSame);
  pInst = _apInst[Unit];
  return pInst->IsInited ? 0 : 1;
}

/*********************************************************************
*
*       _IoCtl
*
*  Function description
*    FS driver function. Execute device command.
*
*  Parameters
*    Unit      Index of the driver instance (0-based).
*    Cmd       Command to be executed.
*    Aux       Parameter depending on command.
*    pBuffer   Pointer to a buffer used for the command.
*
*  Return value
*    Command specific. In general a negative value means an error.
*
*  Additional information
*    A short description of the command parameters can be found in
*    the FS.h header file.
*/
static int _IoCtl(U8 Unit, I32 Cmd, I32 Aux, void * pBuffer) {
  DRIVER_INST * pInst;
  FS_DEV_INFO * pInfo;
  int           r;

  pInst = _apInst[Unit];
  r     = -1;
  FS_USE_PARA(Aux);
  switch (Cmd) {
  case FS_CMD_GET_DEVINFO:
    pInfo = (FS_DEV_INFO *)pBuffer;
    pInfo->NumHeads        = pInst->NumHeads;
    pInfo->SectorsPerTrack = pInst->SectorsPerTrack;
    pInfo->NumSectors      = pInst->NumSectors;
    pInfo->BytesPerSector  = pInst->BytesPerSector;
    r = 0;
    break;
  case FS_CMD_UNMOUNT:
    //
    // (Optional)
    // Device shall be unmounted - sync all operations and mark it as unmounted
    //
    _Unmount(pInst);
    r = 0;
    break;
  case FS_CMD_UNMOUNT_FORCED:
    //
    // (Optional)
    // Device shall be unmounted - mark it as unmounted without syncing any pending operations
    //
    break;
  case FS_CMD_SYNC:
    //
    // (Optional)
    // Sync/flush any pending operations
    //
    break;
  case FS_CMD_FREE_SECTORS:
    //
    // (Optional)
    // The range of logical sectors passed via Aux and pBuffer store invalid data.
    //
    r = 0;
    break;
  default:
    break;
  }
  return r;
}

/*********************************************************************
*
*       _InitMedium
*
*  Function description
*    Initialize the specified medium.
*
*  Parameters
*    Unit   Index of the driver instance (0-based).
*
*  Return value
*    ==0    OK, storage medium initialized.
*    !=0    An error occurred.
*/
static int _InitMedium(U8 Unit) {
  int           r;
  DRIVER_INST * pInst;

  pInst = _apInst[Unit];
  if (pInst->IsInited == 0) {
    r = _Init(pInst);
    if (r) {
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "DRV: Could not initialize the driver.\n"));
      return 1;
    }
    pInst->IsInited = 1;
  }
  return 0;
}

/*********************************************************************
*
*       _GetStatus
*
*  Function description
*    Returns status the presence status of the storage device.
*
*  Parameters
*    Unit     Index of the driver instance (0-based).
*
*  Return value
*    FS_MEDIA_STATE_UNKNOWN   The state of the media is unknown.
*    FS_MEDIA_NOT_PRESENT     No card is present.
*    FS_MEDIA_IS_PRESENT      If a card is present.
*
*  Additional information
*    This function is relevant only for removable storage devices
*    such as SD cards. For other storage devices this function
*    can simply return FS_MEDIA_IS_PRESENT.
*/
static int _GetStatus(U8 Unit) {
  FS_USE_PARA(Unit);
  return FS_MEDIA_IS_PRESENT;
}

/*********************************************************************
*
*       _GetNumUnits
*
*  Function description
*    Returns the number of driver instances.
*
*  Return value
*    >=0    OK, number of driver instances.
*    < 0    An error occurred.
*/
static int _GetNumUnits(void) {
  return _NumUnits;
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_DEVICE_TYPE FS_TEMPLATE_Driver = {
  _GetDriverName,
  _AddDevice,
  _Read,
  _Write,
  _IoCtl,
  _InitMedium,
  _GetStatus,
  _GetNumUnits
};

/*************************** End of file ****************************/
