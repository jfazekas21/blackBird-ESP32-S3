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

File    : FS_ConfigNOR_BM_SPIFI_LPC4322_SEGGER_QSPIFI_Test_Board.c
Purpose : Configuration file for serial NOR flash connected via SPIFI.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include "FS_NOR_HW_SPIFI_LPC4322_SEGGER_QSPIFI_Test_Board.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define ALLOC_SIZE          0x4000        // Size of memory dedicated to the file system. This value should be fine tuned according for your system.
#define FLASH_BASE_ADDR     0x80000000    // Base address of the NOR flash device to be used as storage.
#define FLASH_START_ADDR    0x80000000    // Start address of the first sector be used as storage. If the entire chip is used for file system, it is identical to the base address.
#define FLASH_SIZE          0x10000000    // Number of bytes to be used for storage
#define BYTES_PER_SECTOR    512           // Logical sector size

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*       Memory pool used for semi-dynamic allocation.
*/
#ifdef __ICCARM__
  #pragma location="FS_RAM"
  static __no_init U32 _aMemBlock[ALLOC_SIZE / 4];
#endif
#ifdef __CC_ARM
  static U32 _aMemBlock[ALLOC_SIZE / 4] __attribute__ ((section ("FS_RAM"), zero_init));
#endif
#if (!defined(__ICCARM__) && !defined(__CC_ARM))
  static U32 _aMemBlock[ALLOC_SIZE / 4];
#endif

/*********************************************************************
*
*      Static const data
*
**********************************************************************
*/

static const FS_NOR_SPI_DEVICE_PARA _aMacronix_MX25L25645G = {
  0x19,     // Id               (Third byte in response to READ ID command.)
  0,        // ldBytesPerSector (Not used, detected automatically via SFDP.)
  0,        // ldBytesPerPage   (Not used, fixed.)
  0,        // NumBytesAddr     (Not used, detected automatically via SFDP.)
  0,        // NumSectors:      (Not used, detected automatically via SFDP.)
  0,        // Flags            (Not used and not required.)
  0xC2,     // MfgId            (First byte in response to READ ID command.)
  0,        // CmdWrite112      (Not supported by this device.)
  0,        // CmdWrite122      (Not supported by this device.)
  0,        // CmdWrite114      (Not supported by this device.)
  0x38      // CmdWrite144
};

static const FS_NOR_SPI_DEVICE_PARA_LIST _ParaList = {
  1,
  &_aMacronix_MX25L25645G
};

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
  // Give file system memory to work with.
  //
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  //
  // Configure the size of the logical sector and activate the file buffering.
  //
  FS_SetMaxSectorSize(BYTES_PER_SECTOR);
#if FS_SUPPORT_FILE_BUFFER
  FS_ConfigFileBufferDefault(BYTES_PER_SECTOR, FS_FILE_BUFFER_WRITE);
#endif
  //
  // Add and configure the NOR driver.
  //
  FS_AddDevice(&FS_NOR_BM_Driver);
  FS_NOR_BM_SetPhyType(0, &FS_NOR_PHY_SPIFI);
  FS_NOR_BM_Configure(0, FLASH_BASE_ADDR, FLASH_START_ADDR, FLASH_SIZE);
  FS_NOR_BM_SetSectorSize(0, BYTES_PER_SECTOR);
  FS_NOR_BM_SetEraseVerification(0, 0);
  FS_NOR_BM_SetWriteVerification(0, 0);
#if FS_NOR_SUPPORT_VARIABLE_BYTE_ORDER
  FS_NOR_BM_SetByteOrderBE();
#endif
  //
  // Configure the NOR physical layer.
  //
  FS_NOR_SPIFI_Allow2bitMode(0, 1);
  FS_NOR_SPIFI_Allow4bitMode(0, 1);
  FS_NOR_SPIFI_SetHWType(0, &FS_NOR_HW_SPIFI_LPC4322_SEGGER_QSPIFI_Test_Board);
  FS_NOR_SPIFI_SetDeviceList(0, &FS_NOR_SPI_DeviceListAll);
  FS_NOR_SPIFI_SetDeviceParaList(0, &_ParaList);
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
