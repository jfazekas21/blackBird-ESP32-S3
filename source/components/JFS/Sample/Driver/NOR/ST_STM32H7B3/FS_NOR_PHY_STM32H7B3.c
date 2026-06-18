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

File    : FS_NOR_PHY_STM32H7B3.c
Purpose : NOR physical layer for the internal flash memory of ST STM32H7B3.
Literature:
  [1] RM0455 Reference manual STM32H7A3/7B3 and STM32H7B0 Value line advanced Arm-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\RM0455_STM32H7A3_H7B3_H7B0_rev4.pdf)
  [3] Datasheet STM32H7B3xI
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\DS13139_STM32H7B3xx_rev3.pdf)
  [4] UM2569 User manual Discovery kit with STM32H7B3LI MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\EvalBoard\STM32H7B3I_DK\dm00610478-discovery-kit-with-stm32h7b3li-mcu-stmicroelectronics.pdf)
  [5]  STM32H7A3xI STM32H7A3xG STM32H7B0xB STM32H7B3xI Errata sheet
    (\\FILESERVER\Techinfo_rw\Company\ST\MCU\STM32\STM32H7\ES0478_rev9_STM32H7A3XIG_STM32H7B0XB_STM32H7B3XI_DM00598144.pdf)
Additional information:
  This physical layer expects that the parameters of the internal flash
  memory such as number of wait states are configured by the application.

  The characteristics of the internal flash memory request that the
  data is written 16 bytes at a time and that the write operations
  are aligned to an 16 byte boundary. In addition, it is not possible
  to rewrite a previously programmed flash memory location without
  an erase operation in between. Because of these limitations the
  NOR driver has to be compiled with the following configuration:
    FS_NOR_CAN_REWRITE           = 0
    FS_NOR_LINE_SIZE             = 16
    FS_NOR_OPTIMIZE_HEADER_WRITE = 1
  More information about these configuration defines can be found
  in the emFile manual.

  In addition the following function calls have to be added to
  FS_X_AddDevices() in order to prevent unwanted uncorrectable
  bit errors during the low-level format operation operation of 
  the Block Map NOR driver:

  #if FS_NOR_SKIP_BLANK_SECTORS
  FS_NOR_BM_SetBlankSectorSkip(0, 0);
  #endif
  FS_NOR_BM_SetUsedSectorsErase(0, 1);

  The flash controller generates a BusFault exception when the
  ECC detects a bit error that is not able to correct. This NOR physical
  layer comes with handling for such exceptions that has to be
  explicitly enabled by the application via the FS_NOR_PHY_HANDLE_BIT_ERRORS
  configuration define. The handling is implemented in the HardFaultHandler()
  function that has to be called in the handler of a HardFault exception.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include <stm32h7b3xxq.h>

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_PHY_HANDLE_BIT_ERRORS
  #define FS_NOR_PHY_HANDLE_BIT_ERRORS      0   // Enable/disable the handling of uncorrectable bit errors.
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       FLASH registers
*/
#define FLASH_BASE_ADDR         0x52002000
#define FLASH_ACR               (*(volatile U32*)(FLASH_BASE_ADDR + 0x000))
#define FLASH_KEYR1             (*(volatile U32*)(FLASH_BASE_ADDR + 0x004))
#define FLASH_CR1               (*(volatile U32*)(FLASH_BASE_ADDR + 0x00C))
#define FLASH_SR1               (*(volatile U32*)(FLASH_BASE_ADDR + 0x010))
#define FLASH_CCR1              (*(volatile U32*)(FLASH_BASE_ADDR + 0x014))
#define FLASH_KEYR2             (*(volatile U32*)(FLASH_BASE_ADDR + 0x104))
#define FLASH_CR2               (*(volatile U32*)(FLASH_BASE_ADDR + 0x10C))
#define FLASH_SR2               (*(volatile U32*)(FLASH_BASE_ADDR + 0x110))
#define FLASH_CCR2              (*(volatile U32*)(FLASH_BASE_ADDR + 0x114))

/*********************************************************************
*
*       System Control Block
*/
#define SCB_BASE_ADDR           0xE000E000uL
#define SCB_CCR                 (*(volatile U32 *)(SCB_BASE_ADDR + 0xD14))    // Configuration Control Register
#define SCB_SHCSR               (*(volatile U32 *)(SCB_BASE_ADDR + 0xD24))    // HardFault Status Register
#define SCB_CFSR                (*(volatile U32 *)(SCB_BASE_ADDR + 0xD28))    // BusFault Status Register
#define SCB_HFSR                (*(volatile U32 *)(SCB_BASE_ADDR + 0xD2C))    // HardFault Status Register
#define SCB_BAFR                (*(volatile U32 *)(SCB_BASE_ADDR + 0xD38))    // BusFault Address Register

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
*       Misc. registers
*/
#define SYS_FSIZE               (*(volatile U16*)(0x08FFF80C))

/*********************************************************************
*
*       Flash control register
*/
#define CR_LOCK_BIT             0
#define CR_PG_BIT               1
#define CR_SER_BIT              2
#define CR_START_BIT            5
#define CR_SSN_BIT              6
#define CR_SSN_MASK             0x7FuL

/*********************************************************************
*
*       Flash status register
*/
#define SR_BSY_BIT              0
#define SR_QW_BIT               2
#define SR_EOP_BIT              16
#define SR_WRPERR_BIT           17
#define SR_PGSERR_BIT           18
#define SR_STRBERR_BIT          19
#define SR_INCERR_BIT           21
#define SR_RDPERR_BIT           23
#define SR_RDSERR_BIT           24
#define SR_SNECCERR_BIT         25
#define SR_DBECCERR_BIT         26
#define SR_CRCEND_BIT           27
#define SR_CRCDERR_BIT          28

/*********************************************************************
*
*       Flash unlock keys
*/
#define KEYR_KEY1               0x45670123
#define KEYR_KEY2               0xCDEF89AB

/*********************************************************************
*
*       MPU defines
*/
#define CTRL_ENABLE_BIT         0
#define CTRL_PRIVDEFENA_BIT     2
#define RNR_REGION_MASK         0xFF
#define RASR_ENABLE_BIT         0
#define RASR_SIZE_BIT           1
#define RASR_TEX_BIT            19
#define RASR_AP_BIT             24
#define RASR_XN_BIT             28
#define RASR_AP_FULL            0x3uL
#define TYPE_DREGION_BIT        8
#define TYPE_DREGION_MASK       0xFFuL
#define RBAR_REGION_BIT         0
#define RBAR_REGION_MASK        0x1FuL
#define RBAR_VALID_BIT          5

/*********************************************************************
*
*       Misc. defines
*/
#define LD_BYTES_PER_SECTOR     13    // Size of a physical sector(power of 2 value).
#define CCR_DC_BIT              16
#define CCR_BFHFNMIGN_BIT       8
#define BFSR_BFARVALID_BIT      15
#define BFSR_PRECISEERR_BIT     9
#define SHCSR_BUSFAULTACT_BIT   1
#define SHCSR_BUSFAULTENA_BIT   17
#define HFSR_DEBUGEVT_BIT       31
#define GET_SR(BankIndex)       ((BankIndex == 0) ? &FLASH_SR1 : &FLASH_SR2)
#define GET_CR(BankIndex)       ((BankIndex == 0) ? &FLASH_CR1 : &FLASH_CR2)
#define GET_CCR(BankIndex)      ((BankIndex == 0) ? &FLASH_CCR1 : &FLASH_CCR2)
#define GET_KEYR(BankIndex)     ((BankIndex == 0) ? &FLASH_KEYR1 : &FLASH_KEYR2)
#define WAIT_TIMEOUT_CYCLES     300000000uL   // This is the number of cycles required to create a delay of about 7 seconds at a CPU frequency of 480 MHz

/*********************************************************************
*
*       ASSERT_DATA_IS_ALIGNED
*/
#if (FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL)
  #define ASSERT_DATA_IS_ALIGNED(Data)                                             \
    if (((Data) & (FS_NOR_LINE_SIZE - 1u)) != 0) {                                 \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Data is not aligned.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                         \
    }
#else
  #define ASSERT_DATA_IS_ALIGNED(Data)
#endif

/*********************************************************************
*
*       ASSERT_ADDR_IS_ALIGNED
*/
#if (FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL)
  #define ASSERT_ADDR_IS_ALIGNED(pData)                                               \
    if ((SEGGER_PTR2ADDR(pData) & 3u) != 0) {                                         \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Address is not aligned.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                            \
    }
#else
  #define ASSERT_ADDR_IS_ALIGNED(pData)
#endif

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
static U16 _NumSectors;         // Total number of physical sectors in the internal flash memory.
static U32 _BytesPerSector;     // Number of bytes in a physical sector.
static U32 _BaseAddr;           // Address of the first byte in the internal flash.
static U32 _StartAddrConf;      // Address of the first byte to be used as storage as configured by the application.
static U32 _NumBytesConf;       // Number of bytes to be used as storage as configured by the application.
static U32 _StartAddrUsed;      // Address of the first byte to be used as storage aligned to a physical sector boundary.
static U32 _NumBytesUsed;       // Number of bytes to be used as storage as configured by the application.
static U16 _NumSectorsUsed;     // Number of physical sectors used as storage.

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _ld
*/
static unsigned _ld(U32 Value) {
  unsigned i;

  for (i = 0; i < 32u; i++) {
    if ((1uL << i) == Value) {
      break;
    }
  }
  return i;
}

/*********************************************************************
*
*      _CalcStorageArea
*
*  Function description
*    Determines which physical sectors are used as storage.
*/
static int _CalcStorageArea(void) {
  U16 NumSectors;
  U32 BytesPerSector;
  I32 NumBytesToSkip;
  I32 NumBytesRem;
  U32 NumBytes;
  U32 NumBytesBlock;
  U32 NumBytesSkipped;
  U8  ldBytesPerSector;
  U16 NumSectorsRem;
  U16 NumSectorsUsed;

  NumSectorsUsed   = 0;
  NumBytesToSkip   = (I32)_StartAddrConf - (I32)_BaseAddr;
  NumBytesSkipped  = 0;
  NumBytesRem      = (I32)_NumBytesConf;
  NumBytes         = 0;
  ldBytesPerSector = LD_BYTES_PER_SECTOR;
  NumSectors       = _NumSectors;
  BytesPerSector   = _BytesPerSector;
  //
  // Take care of bytes to skip before storage area.
  //
  while ((NumSectors != 0u) && (NumBytesToSkip > 0)) {
    NumBytesToSkip  -= (I32)BytesPerSector;
    NumBytesSkipped += BytesPerSector;
    NumSectors--;
  }
  if (NumSectors != 0u) {
    NumSectorsRem = (U16)((U32)NumBytesRem >> ldBytesPerSector);
    if (NumSectors > NumSectorsRem) {
      NumSectors   = NumSectorsRem;
    }
    NumBytesBlock  = (U32)NumSectors << ldBytesPerSector;
    NumBytesRem   -= (I32)NumBytesBlock;
    NumBytes      += NumBytesBlock;           // Update the actual number of bytes used as storage.
    //
    // Take care of bytes to skip after data area
    //
    if (NumSectors != 0u) {
      NumSectorsUsed += NumSectors;
    }
  }
  if (NumSectorsUsed == 0u) {
    return 1;                                 // Error, Flash size too small for this configuration
  }
  _NumSectorsUsed = NumSectorsUsed;
  _StartAddrUsed  = _BaseAddr + NumBytesSkipped;
  _NumBytesUsed   = NumBytes;
  return 0;                                   // OK, storage area determined.
}

/*********************************************************************
*
*      _WaitForReady
*/
static int _WaitForReady(unsigned BankIndex) {
  volatile U32 * pSR;
  U32            TimeOut;
  int            r;

  r   = 0;                // Set to indicate success.
  pSR = GET_SR(BankIndex);
  //
  // Wait for the flash memory to become ready for the operation.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((*pSR & (1uL << SR_BSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*      _UnlockIfRequired
*/
static int _UnlockIfRequired(unsigned BankIndex) {
  int            IsLocked;
  volatile U32 * pKEYR;
  volatile U32 * pCR;
  U32            TimeOut;

  pKEYR    = GET_KEYR(BankIndex);
  pCR      = GET_CR(BankIndex);
  IsLocked = 0;
  if ((*pCR & (1uL << CR_LOCK_BIT)) != 0u) {
    IsLocked = 1;
    *pKEYR = KEYR_KEY1;
    *pKEYR = KEYR_KEY2;
    //
    // Wait for the flash to unlock.
    //
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((*pCR & (1uL << CR_LOCK_BIT)) == 0u) {
        break;
      }
      if (--TimeOut == 0u) {
        break;
      }
    }
  }
  return IsLocked;
}

/*********************************************************************
*
*      _Lock
*/
static void _Lock(unsigned BankIndex) {
  volatile U32 * pCR;
  U32            TimeOut;

  pCR   = GET_CR(BankIndex);
  *pCR |= 1uL << CR_LOCK_BIT;
  //
  // Wait for the flash to lock.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((*pCR & (1uL << CR_LOCK_BIT)) != 0u) {
      break;
    }
    if (--TimeOut == 0u) {
      break;
    }
  }
}

/*********************************************************************
*
*      _WaitForEndOfOperation
*/
static int _WaitForEndOfOperation(unsigned BankIndex) {
  int            r;
  U32            Status;
  volatile U32 * pSR;
  U32            TimeOut;

  r       = 1;            // Set to indicate failure.
  pSR     = GET_SR(BankIndex);
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    Status = *pSR;
    if ((Status & (1uL << SR_QW_BIT)) == 0u) {
      break;
    }
    if (--TimeOut == 0u) {
      break;
    }
  }
  if ((Status & (1uL << SR_WRPERR_BIT)) != 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Flash operation failed (Flash is write-protected).\n"));
  } else if ((Status & (1uL << SR_PGSERR_BIT)) != 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Flash operation failed (Sequence error).\n"));
  } else if ((Status & (1uL << SR_STRBERR_BIT)) != 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Flash operation failed (Strobe error).\n"));
  } else if ((Status & (1uL << SR_INCERR_BIT)) != 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Flash operation failed (Inconsistency error).\n"));
  } else if (TimeOut == 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32H7B3: Flash operation failed (Timeout).\n"));
  } else {
    r = 0;
  }
  return r;
}

/*********************************************************************
*
*      _CalcSectorIndex
*
*  Function description
*    Calculates the index of the sector by the byte offset.
*/
static U32 _CalcSectorIndex(U32 Off) {
  U32 SectorIndex;

  SectorIndex = Off >> LD_BYTES_PER_SECTOR;
  return SectorIndex;
}

/*********************************************************************
*
*      _CalcBankIndex
*
*  Function description
*    Calculates the index of the bank by the sector index.
*/
static unsigned _CalcBankIndex(U32 SectorIndex) {
  U32      NumSectorsBank;
  unsigned BankIndex;

  BankIndex      = 0;
  NumSectorsBank = _NumSectors >> 1u;           // Two banks of equal sizes.
  if (SectorIndex >= NumSectorsBank) {
    BankIndex = 1;
  }
  return BankIndex;
}

/*********************************************************************
*
*      _CalcBRSI
*
*  Function description
*    Calculates the bank-relative sector index.
*/
static U32 _CalcBRSI(U32 SectorIndex) {
  U32 NumSectorsBank;

  NumSectorsBank = _NumSectors >> 1u;           // Two banks of equal sizes.
  if (SectorIndex >= NumSectorsBank) {
    SectorIndex -= NumSectorsBank;
  }
  return SectorIndex;
}

/*********************************************************************
*
*       _DisableDCache
*/
static void _DisableDCache(void) {
  U32 NumRegions;
  U32 iRegion;
  U32 Mask;
  U32 ldNumBytes;

  if (SCB_CCR & (1uL << CCR_DC_BIT)) {      // Is data cache enabled?
    NumRegions = (MPU_TYPE >> TYPE_DREGION_BIT) & TYPE_DREGION_MASK;
    //
    // Find the next free region.
    //
    for (iRegion = 0; iRegion < NumRegions; ++iRegion) {
      MPU_RNR = iRegion;
      if ((MPU_RASR & (1uL << RASR_ENABLE_BIT)) == 0) {
        break;            // Found a free region.
      }
      //
      // Check if we already configured the MPU.
      //
      Mask = 0uL
           | (RBAR_REGION_MASK << RBAR_REGION_BIT)
           | (1uL              << RBAR_VALID_BIT)
           ;
      if ((MPU_RBAR & ~Mask) == _StartAddrConf) {
        iRegion = NumRegions;
        break;            // Already configured.
      }
    }
    if (iRegion < NumRegions) {
      ldNumBytes = _ld(_NumBytesConf);
      //
      // Use MPU to disable the data cache on the memory region assigned to QSPI.
      //
      MPU_CTRL &= ~(1uL << CTRL_ENABLE_BIT);      // Disable MPU first.
      MPU_RNR   = iRegion & RNR_REGION_MASK;
      MPU_RBAR  = _StartAddrConf;
      MPU_RASR  = 0
                | (1uL              << RASR_XN_BIT)
                | (RASR_AP_FULL     << RASR_AP_BIT)
                | (1uL              << RASR_TEX_BIT)
                | ((ldNumBytes - 1) << RASR_SIZE_BIT)
                | (1uL              << RASR_ENABLE_BIT)
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
*      _StartErase
*
*  Function description
*    Starts the execution of an erase operation.
*/
static void _StartErase(unsigned BankIndex, U32 brsi) {
  volatile U32 * pCR;
  volatile U32 * pCCR;

  pCR  = GET_CR(BankIndex);
  pCCR = GET_CCR(BankIndex);
  //
  // According to [1] all the errors have to be cleared before starting
  // the operation otherwise the operation will fail.
  //
  *pCCR |= 0u
        | (1uL << SR_EOP_BIT)
        | (1uL << SR_WRPERR_BIT)
        | (1uL << SR_PGSERR_BIT)
        | (1uL << SR_STRBERR_BIT)
        | (1uL << SR_INCERR_BIT)
        | (1uL << SR_RDPERR_BIT)
        | (1uL << SR_RDSERR_BIT)
        | (1uL << SR_SNECCERR_BIT)
        | (1uL << SR_DBECCERR_BIT)
        | (1uL << SR_CRCEND_BIT)
        | (1uL << SR_CRCDERR_BIT)
        ;
  //
  // Set flash controller to sector erase mode and select sector to be erased.
  //
  *pCR &= ~(CR_SSN_MASK << CR_SSN_BIT);
  *pCR |= 0
       | (1uL  << CR_SER_BIT)
       | (brsi << CR_SSN_BIT)
       ;
  //
  // Make sure that the data is written to register.
  //
  __ISB();
  __DSB();
  //
  // Start erasing.
  //
  *pCR |= 1uL << CR_START_BIT;
  //
  // Make sure that the data is written to register.
  //
  __ISB();
  __DSB();
}

/*********************************************************************
*
*      _CompleteErase
*
*   Function description
*     Ends the execution of an erase operation.
*/
static void _CompleteErase(unsigned BankIndex) {
  volatile U32 * pCR;

  pCR = GET_CR(BankIndex);
  *pCR &= ~((1uL         << CR_SER_BIT)   |
            (CR_SSN_MASK << CR_SSN_BIT));
}

/*********************************************************************
*
*      _EraseSector
*
*  Function description
*    Erases one physical sector.
*
*  Parameters
*    SectorIndex  Index of the physical sector to be erased.
*
*  Return value
*    ==0    OK, sector was erased.
*    !=0    Error, sector could not be erased.
*
*  Additional information
*    SectorIndex is relative to the beginning of flash memory range used as storage.
*/
static int _EraseSector(U32 SectorIndex) {
  int      IsLocked;
  int      r;
  unsigned BankIndex;
  U32      brsi;
  U32      StartSector;

  //
  // Convert to absolute sector index.
  //
  StartSector  = (_StartAddrUsed - _BaseAddr) >> LD_BYTES_PER_SECTOR;
  SectorIndex += StartSector;
  //
  // Calculate the bank-relative sector index.
  //
  BankIndex = _CalcBankIndex(SectorIndex);
  brsi      = _CalcBRSI(SectorIndex);
  //
  // Wait for the flash controller to become ready.
  //
  (void)_WaitForReady(BankIndex);
  //
  // If required, unlock the access to control register.
  //
  IsLocked = _UnlockIfRequired(BankIndex);
  //
  // Start the erase operation.
  //
  _StartErase(BankIndex, brsi);
  //
  // Wait for the end of operation.
  //
  r = _WaitForEndOfOperation(BankIndex);
  //
  // Finish the erase operation.
  //
  _CompleteErase(BankIndex);
  //
  // If required, lock again the access to the control register.
  //
  if (IsLocked != 0) {
    _Lock(BankIndex);
  }
  return r;
}

/*********************************************************************
*
*      _EnableProgram
*
*  Function description
*    Starts a programming operation.
*/
static void _EnableProgram(unsigned BankIndex) {
  volatile U32 * pCR;
  volatile U32 * pCCR;

  pCR  = GET_CR(BankIndex);
  pCCR = GET_CCR(BankIndex);
  //
  // According to [1] all the errors have to be cleared before starting
  // the operation otherwise the operation will fail.
  //
  *pCCR |= 0u
        | (1uL << SR_EOP_BIT)
        | (1uL << SR_WRPERR_BIT)
        | (1uL << SR_PGSERR_BIT)
        | (1uL << SR_STRBERR_BIT)
        | (1uL << SR_INCERR_BIT)
        | (1uL << SR_RDPERR_BIT)
        | (1uL << SR_RDSERR_BIT)
        | (1uL << SR_SNECCERR_BIT)
        | (1uL << SR_DBECCERR_BIT)
        | (1uL << SR_CRCEND_BIT)
        | (1uL << SR_CRCDERR_BIT)
        ;
  //
  // Make sure that the data is written to register.
  //
  __ISB();
  __DSB();
  *pCR |= 1uL << CR_PG_BIT;
  //
  // Make sure that the data is written to register.
  //
  __ISB();
  __DSB();
}

/*********************************************************************
*
*      _DisableProgram
*
*  Function description
*    Ends a programming operation.
*/
static void _DisableProgram(unsigned BankIndex) {
  volatile U32 * pCR;

  pCR = GET_CR(BankIndex);
  *pCR &= ~(1uL << CR_PG_BIT);
}

/*********************************************************************
*
*      _WriteOff
*
*  Function description
*    Writes data to internal flash memory.
*
*  Parameters
*    Off        Byte offset to write to.
*    pData      Data to be written.
*    NumBytes   Number of bytes to be written.
*
*  Return value
*    ==0    OK, data was written.
*    !=0    An error occurred.
*
*  Additional information
*    NumBytes has to be multiple of 32 value because this is the
*    number of bytes protected by one ECC parity check.
*/
static int _WriteOff(U32 Off, const void * pData, unsigned NumBytes) {
  int         IsLocked;
  int         r;
  unsigned    BankIndex;
  U32       * pWrite;
  U32         Addr;
  const U32 * pData32;
  unsigned    NumItems;
  U32         SectorIndex;

  //
  // Calculate the address to write to.
  //
  Off         += _StartAddrUsed - _BaseAddr;      // Byte offset relative to beginning of the flash memory.
  SectorIndex  = _CalcSectorIndex(Off);
  BankIndex    = _CalcBankIndex(SectorIndex);
  Addr         = _BaseAddr + Off;                 // Absolute address in system memory.
  pWrite       = SEGGER_ADDR2PTR(U32, Addr);
  pData32      = SEGGER_PTR2PTR(const U32, pData);
  //
  // Wait for the flash controller to become ready.
  //
  (void)_WaitForReady(BankIndex);
  //
  // If required, unlock the access to control register.
  //
  IsLocked = _UnlockIfRequired(BankIndex);
  //
  // Initiate the write operation.
  //
  _EnableProgram(BankIndex);
  //
  // Write the data one flash word (16 bytes) at a time.
  //
  NumItems = NumBytes >> 2;
  for (;;) {
    *pWrite++ = *pData32++;
    *pWrite++ = *pData32++;
    *pWrite++ = *pData32++;
    *pWrite++ = *pData32++;
    //
    // Make sure that the data is written to memory.
    //
    __ISB();
    __DSB();
    NumItems -= 4;
    r = _WaitForEndOfOperation(BankIndex);
    if (r != 0) {
      break;                                      // Error, could not write data.
    }
    if (NumItems == 0) {
      break;                                      // OK, all data was written.
    }
  }
  //
  // Disable the programming operation.
  //
  _DisableProgram(BankIndex);
  //
  // If required, lock again the access to the control register.
  //
  if (IsLocked != 0) {
    _Lock(BankIndex);
  }
  return r;
}

/*********************************************************************
*
*      _ReadOff
*
*  Function description
*    Read data from flash memory and handles the bit errors.
*/
static int _ReadOff(U32 Off, void * pData, U32 NumBytes) {
  const void   * pRead;
  U32            Addr;
  U32            SectorIndex;
  unsigned       BankIndex;
  volatile U32 * pSR;
  int            r;

  r     = 0;                                      // Set to indicate success.
  Addr  = _StartAddrUsed + Off;
  pRead = SEGGER_ADDR2PTR(const void, Addr);
  FS_MEMCPY(pData, pRead, NumBytes);
  //
  // Check for uncorrectable bit errors.
  //
  Off         += _StartAddrUsed - _BaseAddr;      // Byte offset relative to beginning of the flash memory.
  SectorIndex  = _CalcSectorIndex(Off);
  BankIndex    = _CalcBankIndex(SectorIndex);
  pSR = GET_SR(BankIndex);
  if ((*pSR & (1uL << SR_DBECCERR_BIT)) != 0) {
    r = 1;                                        // Error, uncorrectable bit error detected.
  }
  return r;
}

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
  int r;

  ASSERT_DATA_IS_ALIGNED(Off | NumBytes);
  ASSERT_ADDR_IS_ALIGNED(pData);
  FS_USE_PARA(Unit);
  r = 0;                                // Set to indicate success.
  if (NumBytes != 0u) {
    r = _WriteOff(Off, pData, NumBytes);
  }
  return r;
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
*    ==0    O.K., data has been read from flash.
*    !=0    An error occurred.
*/
static int _PHY_ReadOff(U8 Unit, void * pData, U32 Off, U32 NumBytes) {
  int r;

  FS_USE_PARA(Unit);
  r = _ReadOff(Off, pData, NumBytes);
  return r;
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
  int r;

  FS_USE_PARA(Unit);
  r = _EraseSector(SectorIndex);
  return r;
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
  if (pOff != NULL) {
    *pOff = SectorIndex << LD_BYTES_PER_SECTOR;
  }
  if (pLen != NULL) {
    *pLen = _BytesPerSector;
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
  return (int)_NumSectorsUsed;
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
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, StartAddr >= BaseAddr);
  _BaseAddr      = BaseAddr;
  _StartAddrConf = StartAddr;
  _NumBytesConf  = NumBytes;
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
  // Nothing to do here.
  //
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
  U32 FlashSize;
  int r;

  FS_USE_PARA(Unit);
  //
  // Determine the characteristics of the internal flash memory.
  //
  FlashSize   = SYS_FSIZE;                        // The SYS_FSIZE register stores the number of Kbytes.
  FlashSize <<= 10;                               // Calculate the total number of bytes in the internal flash memory.
  //
  // Calculate the total number of physical sectors
  // and the sector size as well as the address
  // of the first byte to be used as storage.
  //
  _BytesPerSector = 1uL       << LD_BYTES_PER_SECTOR;
  _NumSectors     = FlashSize >> LD_BYTES_PER_SECTOR;
  r = _CalcStorageArea();
  //
  // Make sure that the access to the flash region
  // used as storage are not cached.
  //
  _DisableDCache();
  return r;
}

/*********************************************************************
*
*       Public functions
*
**********************************************************************
*/

#if FS_NOR_PHY_HANDLE_BIT_ERRORS

/*********************************************************************
*
*       HardFaultHandler
*
*  Function description
*    Handler for bit error exceptions.
*
*  Parameters
*    pStack     Value of the stack pointer.
*
*  Additional information
*    The flash controller generates a BusFault exception when
*    the ECC detects an uncorrectable bit error. We catch and ignore
*    here the BusFault exception and let _ReadOff() return an error to
*    the NOR driver. This function is the C part of the HardFault handler
*    which is called by the assembler implementation of the handler.
*/
void HardFaultHandler(unsigned int * pStack);
void HardFaultHandler(unsigned int * pStack) {
  //
  // In case we received a hard fault because of a breakpoint instruction, we return.
  // This may happen when using semihosting for printf outputs and no debugger is connected,
  // i.e. when running a "Debug" configuration in release mode.
  //
  if (SCB_HFSR & (1u << HFSR_DEBUGEVT_BIT)) {
    SCB_HFSR |=  (1u << HFSR_DEBUGEVT_BIT);                     // Reset HardFault status.
    //
    // PC is located on stack at SP + 24 bytes. Increment PC to skip break instruction.
    //
    *(pStack + 6u) += 2u;
    return;                                                     // Return to interrupted application.
  }
  //
  // Handle BusFault exceptions generated by the flash controller.
  //
  if ((SCB_CFSR & (1uL << BFSR_BFARVALID_BIT)) != 0) {
    U32            pc;
    unsigned int * pPC;
    const U16    * pInst;
    unsigned       InstType;
    unsigned       InstSize;

    //
    // The flash controller generates only precise BusFault exceptions
    // in case of an uncorrectable bit error.
    //
    if ((SCB_CFSR & (1uL << BFSR_PRECISEERR_BIT)) != 0) {
      //
      // Check that the address where the BusFault exception occurred is in the range used by the file system.
      //
      if (   (SCB_BAFR >= _StartAddrUsed)
          && (SCB_BAFR <  (_StartAddrUsed + _NumBytesUsed))) {
        //
        // Calculate the size of instruction that generated the HardFault.
        // From the ARM v7-M Architecture Reference Manual:
        //   If bits [15:11] of the halfword being decoded take any of the following values,
        //   the halfword is the first halfword of a 32-bit instruction:
        //   - 0b11101.
        //   - 0b11110.
        //   - 0b11111.
        //  Otherwise, the halfword is a 16-bit instruction.
        InstSize = 2;                                           // Assume a 16-bit instruction.
        pPC   = pStack + 6u;                                    // PC that generated the exception is located on stack at SP + 24 bytes.
        pc    = *pPC;
        pInst = SEGGER_ADDR2PTR(const U16, pc);
        InstType   = *pInst;
        InstType >>= 11;
        if (   (InstType == 0x1Du)
            || (InstType == 0x1Eu)
            || (InstType == 0x1Fu)) {
          InstSize = 4;                                         // This is a 32-bit instruction.
        }
        //
        // Increment the PC to skip the load instruction that caused the exception.
        //
        *pPC += InstSize;
        return;                                                 // Return to interrupted application.
      }
    }
  }
  //
  // Wait here for the target to be reset.
  //
  for (;;) {
    ;
  }
}

#endif // FS_NOR_PHY_HANDLE_BIT_ERRORS

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_NOR_PHY_STM32L4R5
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_STM32H7B3 = {
  _PHY_WriteOff,
  _PHY_ReadOff,
  _PHY_EraseSector,
  _PHY_GetSectorInfo,
  _PHY_GetNumSectors,
  _PHY_Configure,
  _PHY_OnSelectPhy,
  NULL,
  NULL,
  _PHY_Init
};

/*************************** End of file ****************************/
