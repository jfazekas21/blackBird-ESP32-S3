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

File    : FS_NOR_PHY_Template.c
Purpose : Physical layer template for the NOR device driver.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define NUM_SECTORS         64
#define BYTES_PER_SECTOR    0x10000

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*      Public code (through callback)
*
**********************************************************************
*/

/*********************************************************************
*
*      _PHY_WriteOff
*
*  Function description
*    This routine writes data into any section of the flash.
*
*  Return value
*    ==0    O.K., data has been written
*    !=0    An error occurred
*
*  Parameters
*    Unit         Index of the physical layer.
*    Off          Byte offset relative to beginning of the region
*                 used as storage where to store the first byte.
*    pData        [OUT] Data to be written to storage device.
*    NumBytes     Number of bytes to be written.
*
*  Additional information
*    This function does not check if the section to be written
*    has been previously erased. This is in the responsibility
*    of the NOR driver. It has to be able to write data into
*    multiple sectors at a time.
*/
static int _PHY_WriteOff(U8 Unit, U32 Off, const void * pData, U32 NumBytes) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(Off);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytes);
  //
  // TBD: Implement here the write operation.
  //
  return 0;
}

/*********************************************************************
*
*       _PHY_ReadOff
*
*  Function description
*    Reads a number of bytes from the specified byte offset.
*
*  Parameters
*    Unit         Index of the physical layer.
*    pData        [OUT] Data read from storage device.
*    Off          Byte offset relative to beginning of the region
*                 used as storage of the first byte to be read.
*    NumBytes     Number of bytes to be read.
*
*  Return value
*    ==0    OK, data was read from flash.
*    !=0    An error occurred.
*/
static int _PHY_ReadOff(U8 Unit, void * pData, U32 Off, U32 NumBytes) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(pData);
  FS_USE_PARA(Off);
  FS_USE_PARA(NumBytes);
  //
  // TBD: Implement here the read operation.
  //
  return 0;
}

/*********************************************************************
*
*      _PHY_EraseSector
*
*  Function description
*    Erases one physical sector.
*
*  Parameters
*    Unit         Index of the physical layer.
*    SectorIndex  Index of the physical sector to be erased.
*
*  Return value
*    ==0    OK, sector was successfully erased.
*    !=0    Error, sector could not be erased.
*
*  Additional information
*    SectorIndex is relative to the StartAddr configured
*    by the application via _PHY_Configure(). The the sector
*    located at the address StartAddr has the index 0,
*    the index of the sector located at StartAddr + SectorSize
*    has the index 1 and so on.
*/
static int _PHY_EraseSector(U8 Unit, unsigned int SectorIndex) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(SectorIndex);
  //
  // TBD: Implement here the erase of the physical sector.
  //
  return 0;
}

/*********************************************************************
*
*       _PHY_GetSectorInfo
*
*  Function description
*    Returns the parameters of the specified physical sector.
*
*  Parameters
*    Unit         Index of the physical layer.
*    SectorIndex  Index of the physical sector relative to the first
*                 sector used a storage.
*    pOff         [OUT] Offset of the first byte in the physical sector
*                 relative to beginning of the region used as storage.
*                 pOff is set to NULL if the NOR driver does not require
*                 this parameter.
*    pLen         [OUT] Number of bytes in the specified physical sector.
*                 pLen is set to NULL if the NOR driver does not require
*                 this parameter.
*
*  Additional information
*    SectorIndex can be different than the index of the actual physical
*    sector if a StartAddr larger than BaseAddr is specified in the call
*    to _PHY_Configure().
*    The NOR driver can request either both parameters or only one of them.
*/
static void _PHY_GetSectorInfo(U8 Unit, unsigned int SectorIndex, U32 * pOff, U32 * pLen) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(SectorIndex);
  //
  // TBD: Implement here the logic for storage devices with different sector sizes.
  //
  if (pOff != NULL) {
    *pOff = SectorIndex * BYTES_PER_SECTOR;
  }
  if (pLen != NULL) {
    *pLen = BYTES_PER_SECTOR;
  }
}

/*********************************************************************
*
*       _PHY_GetNumSectors
*
*  Function description
*    Returns the number of physical sectors that can be used for data storage.
*
*  Parameters
*    Unit         Index of the physical layer.
*
*  Additional information
*    The number of sectors can be smaller than the total number of
*    sectors in the storage device if _PHY_Configure() is called with StartAddr
*    larger than BaseAddr or NumBytes is smaller than the total capacity
*    of the storage device.
*/
static int _PHY_GetNumSectors(U8 Unit) {
  FS_USE_PARA(Unit);
  return NUM_SECTORS;
}

/*********************************************************************
*
*       _PHY_Configure
*
*  Function description
*    Configures a single instance of the driver
*
*  Parameters
*    Unit         Index of the physical layer.
*    BaseAddr     Address of the first byte in the storage device.
*    StartAddr    Address of the first byte to be used as storage.
*    NumBytes     Number of bytes to be used as storage.
*
*  Additional information
*    If required, the function is responsible to align StartAddr
*    and NumBytes to a physical sector boundary. StartAddr has to
*    be aligned to the next ascending physical sector boundary.
*    NumBytes has to be aligned to the next descending physical
*    sector boundary.
*/
static void _PHY_Configure(U8 Unit, U32 BaseAddr, U32 StartAddr, U32 NumBytes) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(BaseAddr);
  FS_USE_PARA(StartAddr);
  FS_USE_PARA(NumBytes);
  //
  // TBD
  //
}

/*********************************************************************
*
*       _PHY_OnSelectPhy
*
*  Function description
*    This function is called during the initialization after
*    this physical layer is assigned to NOR driver.
*
*  Parameters
*    Unit         Index of the physical layer.
*
*  Additional information
*    This is the place where physical layer can get information
*    about the storage device.
*/
static void _PHY_OnSelectPhy(U8 Unit) {
  FS_USE_PARA(Unit);
  //
  // TBD
  //
}

/*********************************************************************
*
*       _PHY_DeInit
*
*  Function description
*    Frees the resources allocated for the driver.
*
*  Parameters
*    Unit         Index of the physical layer.
*
*  Additional information
*    This is the place where any allocated dynamic memory has to be freed.
*/
static void _PHY_DeInit(U8 Unit) {
  FS_USE_PARA(Unit);
  //
  // TBD
  //
}

/*********************************************************************
*
*       _PHY_IsSectorBlank
*
*  Function description
*    Checks if all bytes in a physical sector are set to 0xFF.
*
*  Parameters
*    Unit         Index of the physical layer.
*    SectorIndex  Index of the physical sector to be checked.
*
*  Return value
*    ==0      At least one byte is not set to 0xFF.
*    ==1      All the bytes in the phy. sector are set to 0xFF.
*
*  Additional information
*    This function is optional. If not present, the NOR driver
*    performs the blank-checking operation by reading back
*    the data from the physical sector.
*/
static int _PHY_IsSectorBlank(U8 Unit, unsigned int SectorIndex) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(SectorIndex);
  //
  // TBD
  //
  return 0;
}

/*********************************************************************
*
*       _PHY_Init
*
*  Function description
*    Initializes the physical layer.
*
*  Parameters
*    Unit         Index of the physical layer.
*
*  Return value
*    ==0      OK, physical layer initialized.
*    !=0      An error occurred.
*
*  Additional information
*    This function is optional. If not present, the physical layer
*    has to be initialized at the first call to one of the following
*    functions:
*    - _PHY_WriteOff
*    - _PHY_ReadOff
*    - _PHY_EraseSector
*    - _PHY_GetSectorInfo
*    - _PHY_GetNumSectors
*/
static int _PHY_Init(U8 Unit) {
  FS_USE_PARA(Unit);
  //
  // TBD
  //
  return 0;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_NOR_PHY_Template
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_Template = {
  _PHY_WriteOff,
  _PHY_ReadOff,
  _PHY_EraseSector,
  _PHY_GetSectorInfo,
  _PHY_GetNumSectors,
  _PHY_Configure,
  _PHY_OnSelectPhy,
  _PHY_DeInit,
  _PHY_IsSectorBlank,
  _PHY_Init
};

/*************************** End of file ****************************/
