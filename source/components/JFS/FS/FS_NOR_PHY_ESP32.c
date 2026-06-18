/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : FS_NOR_PHY_ESP32.c
Purpose : NOR physical layer for Espressif ESP32.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"
#include "esp_flash_spi_init.h"


/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_PHY_SECTOR_SIZE_SHIFT
  #define FS_NOR_PHY_SECTOR_SIZE_SHIFT    12                             // Size of the NOR physical sector in bytes as a power of 2 exponent.
#endif

#ifndef   FS_NOR_PHY_PART_TYPE
  #define FS_NOR_PHY_PART_TYPE            ESP_PARTITION_TYPE_DATA         // Type of the  partition to be used as storage.
#endif

#ifndef   FS_NOR_PHY_PART_SUBTYPE
  #define FS_NOR_PHY_PART_SUBTYPE         ESP_PARTITION_SUBTYPE_DATA_FAT  // Subtype of the  partition to be used as storage.
#endif

#ifndef   FS_NOR_PHY_PART_NAME
  #define FS_NOR_PHY_PART_NAME            "storage"                           // Name of the partition to be used as storage (0-terminated string).
#endif

#define 	Total_Flash_Size 				0x1000000						//16MB
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
static U32                     _Off;
static U32                     _NumBytes;
static const esp_partition_t * _pPart = NULL;    // Partition to be used as storage.
  
/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*      Static code (public via callback)
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
*    ==0    OK, data has been written
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
  esp_err_t Result;
  
  FS_USE_PARA(Unit);
//  Off += _Off;
 // printf("\n\n Off=%ld, NumBytes=%ld", Off, NumBytes);
//  printf("\n pData=%s",(char*) pData);
  Result = esp_partition_write(_pPart, Off, pData, NumBytes);
  if (Result != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_ESP32: _PHY_WriteOff: Write failure (Off: 0x%08X, NumBytes: 0x%08X, Error: %s)", Off, NumBytes, esp_err_to_name(Result)));
    return 1;
  }
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
  esp_err_t Result;
  
  FS_USE_PARA(Unit);
 // Off += _Off;
  Result = esp_partition_read(_pPart, Off, pData, NumBytes);
  if (Result != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_ESP32: _PHY_ReadOff: Read failure (Off: 0x%08X, NumBytes: 0x%08X, Error: %s)", Off, NumBytes, esp_err_to_name(Result)));
    return 1;
  }
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
  esp_err_t Result;
  U32       Off;
  U32       BytesPerSector;

  FS_USE_PARA(Unit);
  BytesPerSector = 1uL << FS_NOR_PHY_SECTOR_SIZE_SHIFT;
  Off = SectorIndex << FS_NOR_PHY_SECTOR_SIZE_SHIFT;
 // Off += _Off;
 // printf("ESP32 _PHY_EraseSector BytesPerSector=%ld,  _Off=%08lx, Off= %08lx, _pPart->address = %08x", BytesPerSector, _Off, Off, _pPart->address);
  Result = esp_partition_erase_range(_pPart, Off, BytesPerSector);
//  printf("\n _PHY_EraseSector Result =%d", Result);
  if (Result != 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_ESP32: _PHY_EraseSector: Erase failure (SectorIndex: %u, Error: %s)", SectorIndex, esp_err_to_name(Result)));
    return 1;
  }
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
  U32 BytesPerSector;

  FS_USE_PARA(Unit);
  //
  // TBD: Implement here the logic for storage devices with different sector sizes.
  //
  if (pOff != NULL) {
    *pOff = SectorIndex << FS_NOR_PHY_SECTOR_SIZE_SHIFT;
  }
  if (pLen != NULL) {
    BytesPerSector = 1uL << FS_NOR_PHY_SECTOR_SIZE_SHIFT;
    *pLen = BytesPerSector;
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
  U32 NumSectors;
  
  FS_USE_PARA(Unit);
 // printf("\n _PHY_GetNumSectors _NumBytes=%ld, FS_NOR_PHY_SECTOR_SIZE_SHIFT=%d", _NumBytes, (int)FS_NOR_PHY_SECTOR_SIZE_SHIFT);
  NumSectors = _NumBytes >> FS_NOR_PHY_SECTOR_SIZE_SHIFT;
  return (int)NumSectors;
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
 // printf("\n ESP32 BaseAddr=%8lx, StartAddr=%8lx, NumBytes=%8lx", BaseAddr, StartAddr, NumBytes);

  _Off      = StartAddr;
  _NumBytes = NumBytes;

 // printf("\n _PHY_Configure _NumBytes =%ld   0x%08lx", _NumBytes,_NumBytes );
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
	 int partition_index = 0;
		 esp_err_t err;
		    esp_partition_info_t partition_info;

  FS_USE_PARA(Unit); 
  _pPart = esp_partition_find_first(FS_NOR_PHY_PART_TYPE, FS_NOR_PHY_PART_SUBTYPE, FS_NOR_PHY_PART_NAME);
  if (_pPart == NULL) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_ESP32: _PHY_Init: Could not find partition (Type: 0x%02X, Subtype: 0x%02X, Name: \"%s\")", FS_NOR_PHY_PART_TYPE, FS_NOR_PHY_PART_SUBTYPE, FS_NOR_PHY_PART_NAME));
    return 1;
  }
//  else
//	  printf("\n NOR_ESP32: _PHY_Init: Partition found (Type: 0x%02X, Subtype: 0x%02X, Name: \"%s\") at offset 0x%08x with size 0x%08x, _Off=0x%08lx", FS_NOR_PHY_PART_TYPE, FS_NOR_PHY_PART_SUBTYPE, FS_NOR_PHY_PART_NAME,_pPart->address, _pPart->size, _Off);

  if (_Off >= _pPart->size) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_ESP32: _PHY_Init: Invalid offset (Off: 0x%08X, Size: 0x%08X)", _Off, _pPart->size));
    return 1;
  }
//  if ((((_Off + _NumBytes) > _pPart->size)) &&(_NumBytes < Total_Flash_Size)) {
//    _NumBytes -= _pPart->size - (_Off + _NumBytes);
//  }
//  if(_NumBytes > Total_Flash_Size)
//  {
//	  printf("\n Error!  _NumBytes is greater than 16 MB. So, modify it to 16 MB.");
//	  _NumBytes = Total_Flash_Size;
//  }


  FS_DEBUG_LOG((FS_MTYPE_DRIVER, "NOR_ESP32: PART Type: 0x%02X, Subtype: 0x%02X, Name: \"%s\", Addr: 0x%08X, Size: 0x%08X, Off: 0x%08X, NumBytes: 0x%08X\n",
    FS_NOR_PHY_PART_TYPE, FS_NOR_PHY_PART_SUBTYPE, _pPart->label, _pPart->address, _pPart->size, _Off, _NumBytes));
  //printf("\n  NOR_ESP32: PART Type: 0x%02X, Subtype: 0x%02X, Name: \"%s\", Addr: 0x%08X, Size: 0x%08X, Off: 0x%08lX, NumBytes: 0x%08lX\n",
  //  FS_NOR_PHY_PART_TYPE, FS_NOR_PHY_PART_SUBTYPE, _pPart->label, _pPart->address, _pPart->size, _Off, _NumBytes);
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
*       FS_NOR_PHY_ESP32
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_ESP32 = {
  _PHY_WriteOff,
  _PHY_ReadOff,
  _PHY_EraseSector,
  _PHY_GetSectorInfo,
  _PHY_GetNumSectors,
  _PHY_Configure,
  _PHY_OnSelectPhy,
  _PHY_DeInit,
  NULL,
  _PHY_Init
};

/*************************** End of file ****************************/
