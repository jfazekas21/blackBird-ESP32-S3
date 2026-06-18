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

File    : FS_ConfigNOR_CFI_1x16_STM32H743_ST_STM32H743I_EVAL.c
Purpose : Configuration file for FS with one 16-bit CFI compliant NOR flash device.
Literature:
  [1] RM0433 Reference manual STM32H7x3 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H7x3_RM0433_Rev3.pdf)
  [2] STM32H753xI Errata sheet STM32H753xI rev Y device limitations
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H753_Errata_Rev2.pdf)
  [3] UM2199 User manual Evaluation board with STM32H753XI MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\EvalBoard\STM32H7xxI-EVAL1_UM-RevB.pdf)
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include "stm32h743i_eval_nor.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   ALLOC_SIZE
  #define ALLOC_SIZE          0x2000          // Size of memory dedicated to the file system. This value should be fine tuned according for your system.
#endif

#ifndef   NOR_BASE_ADDR
  #define NOR_BASE_ADDR       0x60000000      // Base address of the NOR flash device to be used as storage
#endif

#ifndef   NOR_START_ADDR
  #define NOR_START_ADDR      0x60000000      // Start address of the first sector be used as storage. If the entire chip is used for file system, it is identical to the base addr.
#endif

#ifndef   NOR_SIZE
  #define NOR_SIZE            0x01000000      // Number of bytes to be used for storage
#endif

#ifndef   BYTES_PER_SECTOR
  #define BYTES_PER_SECTOR    512             // logical sector size.
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       System Control Block
*/
#define SCB_BASE_ADDR           0xE000E000uL
#define SCB_CCR                 (*(volatile U32 *)(SCB_BASE_ADDR + 0xD14))    // Configuration Control Register

/*********************************************************************
*
*       Memory protection unit
*/
#define MPU_BASE_ADDR           0xE000ED90uL
#define MPU_TYPE                (*(volatile U32 *)(MPU_BASE_ADDR + 0x00))
#define MPU_CTRL                (*(volatile U32 *)(MPU_BASE_ADDR + 0x04))
#define MPU_RNR                 (*(volatile U32 *)(MPU_BASE_ADDR + 0x08))
#define MPU_RBAR                (*(volatile U32 *)(MPU_BASE_ADDR + 0x0C))
#define MPU_RASR                (*(volatile U32 *)(MPU_BASE_ADDR + 0x10))

/*********************************************************************
*
*       MPU related defines
*/
#define CTRL_ENABLE_BIT         0
#define CTRL_PRIVDEFENA_BIT     2
#define RNR_REGION_MASK         0xFF
#define RASR_ENABLE_BIT         0
#define RASR_SIZE_BIT           1
#define RASR_B_BIT              16
#define RASR_S_BIT              18
#define RASR_TEX_BIT            19
#define RASR_AP_BIT             24
#define RASR_XN_BIT             28
#define RASR_AP_FULL            0x3uL
#define TYPE_DREGION_BIT        8
#define TYPE_DREGION_MASK       0xFFuL
#define CCR_DC_BIT              16

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
  static U32 _aMemBlock[ALLOC_SIZE / 4] __attribute__ ((section ("FS_RAM"), zero_init));
#endif
#ifdef __SES_ARM
  static U32 _aMemBlock[ALLOC_SIZE / 4] __attribute__ ((section ("FS_RAM")));
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

/*********************************************************************
*
*       _InitHW
*
*  Function description
*    Initializes the FMC and disables the data cache.
*/
static void _InitHW(void) {
  U32 NumRegions;
  U32 iRegion;

  //
  // Configure FMC to access the NOR flash device.
  //
  (void)BSP_NOR_Init();
  //
  // Disable the data cache for the memory region
  // where the NOR flash device is mapped.
  //
  if (SCB_CCR & (1uL << CCR_DC_BIT)) {      // Is data cache enabled?
    NumRegions = (MPU_TYPE >> TYPE_DREGION_BIT) & TYPE_DREGION_MASK;
    //
    // Find the next free region.
    //
    for (iRegion = 0; iRegion < NumRegions; ++iRegion) {
      MPU_RNR = iRegion;
      if ((MPU_RASR & (1uL << RASR_ENABLE_BIT)) == 0) {
        break;                                      // Found a free region.
      }
    }
    if (iRegion < NumRegions) {
      //
      // Use MPU to disable the data cache on the memory region assigned to QSPI.
      //
      MPU_CTRL &= ~(1uL << CTRL_ENABLE_BIT);        // Disable MPU first.
      MPU_RNR   = iRegion & RNR_REGION_MASK;
      MPU_RBAR  = NOR_BASE_ADDR;
      MPU_RASR  = 0
                | (RASR_AP_FULL << RASR_AP_BIT)
                | ((24uL - 1uL) << RASR_SIZE_BIT)   // 16 MB block
                | (1uL          << RASR_ENABLE_BIT)
                ;
      //
      // Enable MPU.
      //
      MPU_CTRL |= 0
               | (1uL << CTRL_PRIVDEFENA_BIT)
               | (1uL << CTRL_ENABLE_BIT)
               ;
    }
  }
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
  //
  // Initialize the hardware.
  //
  _InitHW();
  //
  // Give the file system memory to work with.
  //
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  //
  // Add and configure the driver.
  //
  FS_AddDevice(&FS_NOR_BM_Driver);
  FS_NOR_BM_SetPhyType(0, &FS_NOR_PHY_CFI_1x16);
  FS_NOR_BM_Configure(0, NOR_BASE_ADDR, NOR_START_ADDR, NOR_SIZE);
  FS_NOR_BM_SetSectorSize(0, BYTES_PER_SECTOR);
  //
  // Enable the file buffer to increase the performance when reading/writing a small number of bytes.
  //
#if FS_SUPPORT_FILE_BUFFER
  FS_ConfigFileBufferDefault(BYTES_PER_SECTOR, FS_FILE_BUFFER_WRITE);
#endif
  //
  // Configure the file system for fast operation.
  //
  FS_SetFileWriteMode(FS_WRITEMODE_FAST);
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
