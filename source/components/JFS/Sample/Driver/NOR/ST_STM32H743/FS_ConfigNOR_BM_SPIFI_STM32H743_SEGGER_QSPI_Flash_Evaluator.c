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

File        : FS_ConfigNOR_BM_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator.c
Purpose     : Configuration file for serial NOR flash connected via QSPI.
*/

#include "FS.h"
#include "FS_NOR_HW_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   ALLOC_SIZE
  #define ALLOC_SIZE          0x2000            // Size of memory dedicated to the file system. This value should be fine tuned according for your system.
#endif

#ifndef   NOR_BASE_ADDR
  #define NOR_BASE_ADDR       0x90000000        // Base address of the NOR flash device to be used as storage.
#endif

#ifndef   NOR_START_ADDR
  #define NOR_START_ADDR      0x90000000        // Start address of the first sector be used as storage. If the entire chip is used for file system, it is identical to the base address.
#endif

#ifndef   NOR_SIZE
  #define NOR_SIZE            0x10000000        // Number of bytes to be used for storage
#endif

#ifndef   BYTES_PER_SECTOR
  #define BYTES_PER_SECTOR    512               // Logical sector size
#endif

#ifndef   ALLOW_2BIT_MODE
  #define ALLOW_2BIT_MODE     1                 // Enables/disables the data transfer via 2 data lines.
#endif

#ifndef   ALLOW_4BIT_MODE
  #define ALLOW_4BIT_MODE     1                 // Enables/disables the data transfer via 4 data lines.
#endif

#ifndef   FAIL_SAFE_ERASE
  #define FAIL_SAFE_ERASE     1                 // Enables/disables the fail-safe erase procedure.
#endif

#ifndef   CHECK_WRITE
  #define CHECK_WRITE         0                 // Enables/disables the write verification.
#endif

#ifndef   CHECK_ERASE
  #define CHECK_ERASE         0                 // Enables/disables the erase verification.
#endif

#ifndef   PHY_SECTOR_SIZE
  #define PHY_SECTOR_SIZE     0                 // Size of the physical sector size to be used. 0 means use the largest supported by the NOR flash device.
#endif

#ifndef   ALLOW_DTR_MODE
  #define ALLOW_DTR_MODE      0                 // Enable / disable the data transfer on both clock edges.
#endif

#ifndef   BYTES_PER_LINE
  #define BYTES_PER_LINE      FS_NOR_LINE_SIZE  // Number of bytes that have to be written at once.
#endif

/*********************************************************************
*
*       Static const data
*
**********************************************************************
*/

/*********************************************************************
*
*       _apDeviceAll
*/
static const FS_NOR_SPI_TYPE * _apDeviceAll[] = {
  &FS_NOR_SPI_DeviceMicron,
  &FS_NOR_SPI_DeviceMicron_x2,
  &FS_NOR_SPI_DeviceSpansion,
  &FS_NOR_SPI_DeviceMicrochip,
  &FS_NOR_SPI_DeviceWinbondDTR,
  &FS_NOR_SPI_DeviceWinbond,
  &FS_NOR_SPI_DeviceISSIEnhanced,
  &FS_NOR_SPI_DeviceISSIStandard,
  &FS_NOR_SPI_DeviceISSILegacy,
  &FS_NOR_SPI_DeviceMacronix,
  &FS_NOR_SPI_DeviceMacronixOctal,
  &FS_NOR_SPI_DeviceGigaDeviceEnhanced,
  &FS_NOR_SPI_DeviceGigaDeviceStandard,
  &FS_NOR_SPI_DeviceGigaDeviceLowVoltage,
  &FS_NOR_SPI_DeviceCypress,
  &FS_NOR_SPI_DeviceAdestoStandard,
  &FS_NOR_SPI_DeviceAdestoEnhanced,
  &FS_NOR_SPI_DeviceEon,
  &FS_NOR_SPI_DeviceDefault
};

/*********************************************************************
*
*       _DeviceListAll
*/
const FS_NOR_SPI_DEVICE_LIST _DeviceListAll = {
  (U8)SEGGER_COUNTOF(_apDeviceAll),
  _apDeviceAll
};

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
#ifdef __SES_ARM
  static U32 _aMemBlock[ALLOC_SIZE / 4] __attribute__ ((section (".RAM1.FS")));
#endif
#if (!defined(__ICCARM__) && !defined(__CC_ARM) && !defined(__SES_ARM))
  static U32 _aMemBlock[ALLOC_SIZE / 4];
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

#if FS_NOR_SUPPORT_VARIABLE_LINE_SIZE

/*********************************************************************
*
*       _ld
*/
static unsigned _ld(U32 Value) {
  unsigned i;

  for (i = 0; i < 16u; i++) {
    if ((1uL << i) == Value) {
      break;
    }
  }
  return i;
}

#endif // FS_NOR_SUPPORT_VARIABLE_LINE_SIZE

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
  FS_NOR_BM_Configure(0, NOR_BASE_ADDR, NOR_START_ADDR, NOR_SIZE);
  FS_NOR_BM_SetSectorSize(0, BYTES_PER_SECTOR);
#if FS_NOR_SUPPORT_CRC
  FS_NOR_BM_EnableCRC();
#endif // FS_NOR_SUPPORT_CRC
#if FS_NOR_SUPPORT_ECC
  FS_NOR_BM_EnableECC(0);
#endif // FS_NOR_SUPPORT_ECC
#if FS_NOR_SUPPORT_FAIL_SAFE_ERASE
  FS_NOR_BM_SetFailSafeErase(0, FAIL_SAFE_ERASE);
#endif // FS_NOR_SUPPORT_FAIL_SAFE_ERASE
#if FS_NOR_VERIFY_WRITE
  FS_NOR_BM_SetWriteVerification(0, CHECK_WRITE);
#endif // FS_NOR_VERIFY_WRITE
#if FS_NOR_VERIFY_ERASE
  FS_NOR_BM_SetEraseVerification(0, CHECK_ERASE);
#endif // FS_NOR_VERIFY_ERASE
#if FS_NOR_SUPPORT_VARIABLE_LINE_SIZE
  FS_NOR_BM_SetDeviceLineSize(0, _ld(BYTES_PER_LINE));
#endif // FS_NOR_SUPPORT_VARIABLE_LINE_SIZE
  //
  // Configure the NOR physical layer.
  //
  FS_NOR_SPIFI_SetHWType(0, &FS_NOR_HW_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator);
  FS_NOR_SPIFI_SetDeviceList(0, &_DeviceListAll);
  FS_NOR_SPIFI_Allow2bitMode(0, ALLOW_2BIT_MODE);
  FS_NOR_SPIFI_Allow4bitMode(0, ALLOW_4BIT_MODE);
#if PHY_SECTOR_SIZE
  FS_NOR_SPIFI_SetSectorSize(0, PHY_SECTOR_SIZE);
#endif // PHY_SECTOR_SIZE
  FS_NOR_SPIFI_AllowDTRMode(0, ALLOW_DTR_MODE);
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
