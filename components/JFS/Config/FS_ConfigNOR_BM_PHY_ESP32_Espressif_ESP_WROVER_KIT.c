/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : FS_ConfigNOR_BM_PHY_ESP32_Espressif_ESP_WROVER_KIT.c
Purpose : emFile configuration using Block Map NOR driver and NOR physical layer for ESP32.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include "FS_NOR_PHY_ESP32.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   ALLOC_SIZE
  #define ALLOC_SIZE          0x8000          // Size of memory dedicated to the file system. This value should be fine tuned according for your system.
#endif

#ifndef   NOR_BASE_ADDR  
  #define NOR_BASE_ADDR       0x590000       // Base address of the NOR flash device to be used as storage.
#endif
  
#ifndef   NOR_START_ADDR
  #define NOR_START_ADDR      0x590000       // Start address of the first sector be used as storage. If the entire device is used for file system, this value is identical to the base address.
#endif

#ifndef   NOR_SIZE
  #define NOR_SIZE            0x5CC000       // Number of bytes to be used as storage.
#endif

#ifndef   LOG_SECTOR_SIZE
  #define LOG_SECTOR_SIZE     512         // Size of a logical sector in bytes.
#endif  									// Note: for LOG_SECTOR_SIZE = 4096 & ALLOC_SIZE = 0x4000, Error:Could not allocate memory (NumBytesReq: 3798, NumBytesAvail: 3696....)
										// Actual sector size is 4096 which is decided by FS_NOR_PHY_SECTOR_SIZE_SHIFT = 12  (sector size = 2^12).
#ifndef ON
  #define ON 1
#endif

#ifndef OFF
  #define OFF 0
#endif
/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
//static U32 _aMemBlock[ALLOC_SIZE / 4];    // Memory pool used for semi-dynamic allocation.
__attribute__((section(".ext_ram.data"))) static  U32 _aMemBlock[ALLOC_SIZE / 4];

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_X_AddDevices
*
*  Function description
*    This function is called by the FS during FS_Init().
*    It is supposed to add all devices, using primarily FS_AddDevice().
*
*  Note
*    (1) Other API functions may NOT be called, since this function is called
*        during initialization. The devices are not yet ready at this point.
*/
void FS_X_AddDevices(void) {
  //
  // Give the file system memory to work with.
  //
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  //
  // Add and configure the driver.
  //
  FS_AddDevice(&FS_NOR_BM_Driver);
  FS_NOR_BM_SetPhyType(0, &FS_NOR_PHY_ESP32);
  FS_NOR_BM_Configure(0, NOR_BASE_ADDR, NOR_START_ADDR, NOR_SIZE);
  FS_NOR_BM_SetSectorSize(0, LOG_SECTOR_SIZE);

  FS_NOR_BM_SetEraseVerification(0,ON);
  FS_NOR_BM_SetWriteVerification(0,ON);

  //
  // Enable the file buffer to increase the performance when reading/writing a small number of bytes.
  //
#if FS_SUPPORT_FILE_BUFFER
  FS_ConfigFileBufferDefault(LOG_SECTOR_SIZE, FS_FILE_BUFFER_WRITE);
#endif // FS_SUPPORT_FILE_BUFFER
}

/*********************************************************************
*
*       FS_X_GetTimeDate
*
*  Function description
*    Current time and date in a format suitable for the file system.
*
*  Additional information
*    Bit 0-4:   2-second count (0-29)
*    Bit 5-10:  Minutes (0-59)
*    Bit 11-15: Hours (0-23)
*    Bit 16-20: Day of month (1-31)
*    Bit 21-24: Month of year (1-12)
*    Bit 25-31: Count of years from 1980 (0-127)
*/
U32 FS_X_GetTimeDate(void) {
  U32 r;
  U16 Sec, Min, Hour;
  U16 Day, Month, Year;

  Sec   = 0;        // 0 based.  Valid range: 0..59
  Min   = 0;        // 0 based.  Valid range: 0..59
  Hour  = 0;        // 0 based.  Valid range: 0..23
  Day   = 1;        // 1 based.    Means that 1 is 1. Valid range is 1..31 (depending on month)
  Month = 1;        // 1 based.    Means that January is 1. Valid range is 1..12.
  Year  = 0;        // 1980 based. Means that 2007 would be 27.
  r   = Sec / 2 + (Min << 5) + (Hour  << 11);
  r  |= (U32)(Day + (Month << 5) + (Year  << 9)) << 16;
  return r;
}

/*************************** End of file ****************************/
