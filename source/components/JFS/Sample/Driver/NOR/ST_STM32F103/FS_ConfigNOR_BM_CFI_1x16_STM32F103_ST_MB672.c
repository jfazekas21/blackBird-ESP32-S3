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

File    : FS_ConfigNOR_CFI_1x16_STM32F103_ST_MB672.c
Purpose : Configuration file for FS with 1 * 16bit CFI compliant NOR flash
*/

#include "FS.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define ALLOC_SIZE         0x4300         // Size of memory dedicated to the file system. This value should be fine tuned according for your system.
#define FLASH0_BASE_ADDR   0x64000000     // Base addr of the NOR flash device to be used as storage
#define FLASH0_START_ADDR  0x64000000     // Start addr of the first sector be used as storage. If the entire chip is used for file system, it is identical to the base addr.
#define FLASH0_SIZE        0x00400000     // Number of bytes to be used for storage
#define BYTES_PER_SECTOR   512            // Logical sector size in bytes

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       RCC
*/
#define RCC_BASE_ADDR         0x40021000
#define RCC_AHBENR_OFFS       0x14
#define RCC_APB2ENR_OFFS      0x18
#define RCC_AHBENR            (*(volatile U32*)(RCC_BASE_ADDR + RCC_AHBENR_OFFS))
#define RCC_APB2ENR           (*(volatile U32*)(RCC_BASE_ADDR + RCC_APB2ENR_OFFS))
#define RCC_AHB_FSMC_BIT      8
#define RCC_APB_GPIOD_BIT     5
#define RCC_APB_GPIOE_BIT     6
#define RCC_APB_GPIOF_BIT     7
#define RCC_APB_GPIOG_BIT     8

/*********************************************************************
*
*       GPIO
*/
#define GPIO_BASE_ADDR        0x40010800
#define GPIOD_CRL_OFFS        0x0C00
#define GPIOD_CRH_OFFS        0x0C04
#define GPIOE_CRL_OFFS        0x1000
#define GPIOE_CRH_OFFS        0x1004
#define GPIOF_CRL_OFFS        0x1400
#define GPIOF_CRH_OFFS        0x1404
#define GPIOG_CRL_OFFS        0x1800
#define GPIOG_CRH_OFFS        0x1804
#define GPIOD_CRL             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOD_CRL_OFFS))
#define GPIOD_CRH             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOD_CRH_OFFS))
#define GPIOE_CRL             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOE_CRL_OFFS))
#define GPIOE_CRH             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOE_CRH_OFFS))
#define GPIOF_CRL             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOF_CRL_OFFS))
#define GPIOF_CRH             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOF_CRH_OFFS))
#define GPIOG_CRL             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOG_CRL_OFFS))
#define GPIOG_CRH             (*(volatile U32*)(GPIO_BASE_ADDR + GPIOG_CRH_OFFS))

/*********************************************************************
*
*       FSMC
*/
#define FSMC_BASE_ADDR        0xA0000000
#define FSMC_BCR2_OFFS        0x0008
#define FSMC_BTR2_OFFS        0x000C
#define FSMC_BWTR2_OFFS       0x010C
#define FSMC_BCR2             (*(volatile U32*)(FSMC_BASE_ADDR + FSMC_BCR2_OFFS))
#define FSMC_BTR2             (*(volatile U32*)(FSMC_BASE_ADDR + FSMC_BTR2_OFFS))
#define FSMC_BWTR2            (*(volatile U32*)(FSMC_BASE_ADDR + FSMC_BWTR2_OFFS))
#define FSMC_MBKEN_BIT        0
#define FSMC_MWID_BIT         4
#define FSMC_WREN_BIT         12
#define FSMC_ADDSET_BIT       0
#define FSMC_DATAST_BIT       8
#define FSMC_MTYP_BIT         2
#define FSMC_FACCEN_BIT       6
#define FSMC_ACCMOD_BIT       28

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*       Memory pool used for semi-dynamic allocation
*/
#ifdef __ICCARM__
  #pragma location="FS_RAM"
  static __no_init U32 _aMemBlock[ALLOC_SIZE / 4];
#endif
#ifdef __CC_ARM
  U32 static _aMemBlock[ALLOC_SIZE / 4] __attribute__ ((section ("FS_RAM"), zero_init));
#endif
#if (!defined(__ICCARM__) && !defined(__CC_ARM))
  static U32 _aMemBlock[ALLOC_SIZE / 4];
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _InitNOR
*
*   Function description
*     Sets up the interface to NOR flash.
*/
static void _InitNOR(void) {
  //
  // Enable the clocks of used peripherals
  //
  RCC_AHBENR  |= (1uL << RCC_AHB_FSMC_BIT);
  RCC_APB2ENR |= (1uL << RCC_APB_GPIOD_BIT);
  RCC_APB2ENR |= (1uL << RCC_APB_GPIOE_BIT);
  RCC_APB2ENR |= (1uL << RCC_APB_GPIOF_BIT);
  RCC_APB2ENR |= (1uL << RCC_APB_GPIOG_BIT);
  //
  // NOR data lines configuration.
  //
  // The following pins are used:
  // Port D: 0, 1, 8, 9, 10, 14, 15
  // Port E: 7, 8, 9, 10, 11, 12, 13, 14, 15
  // The pins are configured as push-pull at 50MHz
  //
  GPIOD_CRL &= ~0x000000FFuL;
  GPIOD_CRL |=  0x000000BBuL;
  GPIOD_CRH &= ~0xFF000FFFuL;
  GPIOD_CRH |=  0xBB000BBBuL;
  GPIOE_CRL &= ~0xF0000000uL;
  GPIOE_CRL |=  0xB0000000uL;
  GPIOE_CRH &= ~0xFFFFFFFFuL;
  GPIOE_CRH |=  0xBBBBBBBBuL;
  //
  // NOR Address lines configuration
  //
  // The following pins are used:
  // Port D: 11, 12, 13
  // Port E: 3, 4, 5, 6
  // Port F: 0, 1, 2, 3, 4, 5, 12, 13, 14, 15
  // Port G: 0, 1, 2, 3, 4, 5
  // The pins are configured as push-pull at 50MHz
  //
  GPIOD_CRH &= ~0x00FFF000uL;
  GPIOD_CRH |=  0x00BBB000uL;
  GPIOE_CRL &= ~0x0FFFF000uL;
  GPIOE_CRL |=  0x0BBBB000uL;
  GPIOF_CRL &= ~0x00FFFFFFuL;
  GPIOF_CRL |=  0x00BBBBBBuL;
  GPIOF_CRH &= ~0xFFFF0000uL;
  GPIOF_CRH |=  0xBBBB0000uL;
  GPIOG_CRL &= ~0x00FFFFFFuL;
  GPIOG_CRL |=  0x00BBBBBBuL;
  //
  // NOE and NWE configuration: Pins 4 and 5 of port D
  //
  GPIOD_CRL &= ~0x00FF0000uL;
  GPIOD_CRL |=  0x00BB0000uL;
  //
  // NE2 configuration: Pin 9 of port G
  //
  GPIOG_CRH &= ~0x000000F0uL;
  GPIOG_CRH |=  0x000000B0uL;
  //
  // Configure NOR memory Ready/Busy signal: Pin 6 of port G as floating
  //
  GPIOD_CRL &= ~0x0F000000uL;
  GPIOD_CRL |=  0x04000000uL;
  //
  // FSMC Configuration
  //
  FSMC_BCR2   = 0
                | (2uL << FSMC_MTYP_BIT)
                | (1uL << FSMC_MWID_BIT)
                | (1uL << FSMC_WREN_BIT)
                | (1uL << FSMC_FACCEN_BIT)
                ;
  FSMC_BTR2   = 0
                | (2uL << FSMC_ADDSET_BIT)
                | (5uL << FSMC_DATAST_BIT)
                | (1uL << FSMC_ACCMOD_BIT)
                ;
  FSMC_BWTR2  = 0
                | (2uL << FSMC_ADDSET_BIT)
                | (5uL << FSMC_DATAST_BIT)
                | (1uL << FSMC_ACCMOD_BIT)
                ;
  FSMC_BCR2  |=   (1uL << FSMC_MBKEN_BIT);  // Enable the memory bank
}

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
  _InitNOR();
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  FS_SetMaxSectorSize(BYTES_PER_SECTOR);
  //
  // Add and configure the driver.
  //
  FS_AddDevice(&FS_NOR_BM_Driver);
  FS_NOR_BM_SetPhyType(0, &FS_NOR_PHY_CFI_1x16);
  FS_NOR_BM_Configure(0, FLASH0_BASE_ADDR, FLASH0_START_ADDR, FLASH0_SIZE);
  FS_NOR_BM_SetSectorSize(0, BYTES_PER_SECTOR);
  //
  // Enable the file buffer to increase the performance when reading/writing a small number of bytes.
  //
#if FS_SUPPORT_FILE_BUFFER
  FS_ConfigFileBufferDefault(512, FS_FILE_BUFFER_WRITE);
#endif
}

/*********************************************************************
*
*       FS_X_GetTimeDate
*
* Function description
*   Current time and date in a format suitable for the file system.
*
* Additional information
*   Bit 0-4:   2-second count (0-29)
*   Bit 5-10:  Minutes (0-59)
*   Bit 11-15: Hours (0-23)
*   Bit 16-20: Day of month (1-31)
*   Bit 21-24: Month of year (1-12)
*   Bit 25-31: Count of years from 1980 (0-127)
*/
U32 FS_X_GetTimeDate(void) {
  U32 r;
  U16 Sec, Min, Hour;
  U16 Day, Month, Year;

  Sec    = 0;  // 0 based.  Valid range: 0..59
  Min    = 0;  // 0 based.  Valid range: 0..59
  Hour   = 0;  // 0 based.  Valid range: 0..23
  Day    = 1;  // 1 based.    Means that 1 is 1. Valid range is 1..31 (depending on month)
  Month  = 1;  // 1 based.    Means that January is 1. Valid range is 1..12.
  Year   = 0;  // 1980 based. Means that 2007 would be 27.
  r   = Sec / 2 + (Min << 5) + (Hour  << 11);
  r  |= (U32)(Day + (Month << 5) + (Year  << 9)) << 16;
  return r;
}

/*************************** End of file ****************************/
