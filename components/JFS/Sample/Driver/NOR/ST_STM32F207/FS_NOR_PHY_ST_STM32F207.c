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

File    : FS_NOR_PHY_ST_STM32F207.c
Purpose : Physical layer for the internal flash of ST STM32F207 MCU.
Literature:
  [1] RM0033 Reference manual STM32F205xx, STM32F207xx, STM32F215xx and STM32F217xx advanced ARM-based 32-bit MCUs
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32F2\STM32F2xx_RM_rev4_1112.pdf)
  [2] PM0059 Programming manual STM32F205xx, STM32F207xx, STM32F215xx, STM32F217xx
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32F2\STM32F2xx_FlashProgramming_Rev4_1105.pdf)
Additional information:
  This physical layer expects that the parameters of the internal flash
  memory such as number of wait states and the dual bank mode are already
  configured correctly.

  The FS_NOR_PHY_MAX_NUM_BYTES define must be is set according to the supply
  voltage of the MCU as described in the following table taken from [2]

  Supply voltage   FS_NOR_PHY_MAX_NUM_BYTES
  (V)              (bytes)
  -----------------------------------------
  2.7 - 3.6        4
  2.1 - 2.7        2
  1.8 - 2.1        1
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include "FS.h"
#include <stm32f207xx.h>

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_PHY_MAX_NUM_BYTES
  #define FS_NOR_PHY_MAX_NUM_BYTES      4   // Maximum number of bytes that can be written at once.
#endif

#ifndef   FS_NOR_PHY_USE_OS
  #define FS_NOR_PHY_USE_OS             0   // 0: polling mode, 1: event driven
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_PHY_USE_OS
  #include "FS_OS.h"
  #include "RTOS.h"
#endif

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       Flash interface registers
*/
#define FLASH_BASE_ADDR         0x40023C00uL
#define FLASH_ACR               (*(volatile U32 *)(FLASH_BASE_ADDR + 0x00))
#define FLASH_KEYR              (*(volatile U32 *)(FLASH_BASE_ADDR + 0x04))
#define FLASH_OPTKEYR           (*(volatile U32 *)(FLASH_BASE_ADDR + 0x08))
#define FLASH_SR                (*(volatile U32 *)(FLASH_BASE_ADDR + 0x0C))
#define FLASH_CR                (*(volatile U32 *)(FLASH_BASE_ADDR + 0x10))
#define FLASH_OPTCR             (*(volatile U32 *)(FLASH_BASE_ADDR + 0x14))

/*********************************************************************
*
*       Flash keys
*/
#define KEYR_KEY1               0x45670123
#define KEYR_KEY2               0xCDEF89AB

/*********************************************************************
*
*       Status bits
*/
#define SR_EOP_BIT              0
#define SR_OPERR_BIT            1
#define SR_WRPERR_BIT           4
#define SR_PGAERR_BIT           5
#define SR_PGPERR_BIT           6
#define SR_PGSERR_BIT           7
#define SR_BUSY_BIT             16

/*********************************************************************
*
*       Control bits
*/
#define CR_PG_BIT               0     // Start programming
#define CR_SER_BIT              1
#define CR_SNB_BIT              3
#define CR_SNB_MASK             0xFuL
#define CR_PSIZE_BIT            8
#define CR_PSIZE_8              0uL
#define CR_PSIZE_16             1uL
#define CR_PSIZE_32             2uL
#define CR_PSIZE_MASK           0x3uL
#define CR_STRT_BIT             16
#define CR_EOPIE_BIT            24
#define CR_ERRIE_BIT            25
#define CR_LOCK_BIT             31    // Register access protection

/*********************************************************************
*
*       Flash acceleration bits
*/
#define ACR_DCEN_BIT            10
#define ACR_DCRST_BIT           12

/*********************************************************************
*
*       Misc. defines
*/
#define MAX_SECTOR_BLOCKS       3             // Number of different sector sizes.
#define WAIT_TIMEOUT_MS         70000
#define WAIT_TIMEOUT_CYCLES     100000000uL   // This is the number of cycles required to create a delay of about 7 seconds at a CPU frequency of 120 MHz
#define FLASH_PRIO              15

/*********************************************************************
*
*       ASSERT_SECTOR_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)                              \
    if (SectorIndex >= _NumSectorsUsed) {                                           \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Invalid sector index.\n")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                          \
    }
#else
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)
#endif

/*********************************************************************
*
*      Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*      SECTOR_BLOCK
*
*  A sector block defines a number of adjacent sectors of the same size.
*  It therefor contains the size (of one sector) and number of sectors in the block.
*  The entire sectorization can be described with just a few sector blocks.
*/
typedef struct {
  U32 SectorSize;
  U32 NumSectors;
} SECTOR_BLOCK;

/*********************************************************************
*
*      Static const
*
**********************************************************************
*/
static const SECTOR_BLOCK _aSectorBlockDevice[MAX_SECTOR_BLOCKS] = {   // This is the organization of the internal flash.
  {16  * 1024, 4},
  {64  * 1024, 1},
  {128 * 1024, 7}
};

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static U32            _BaseAddr;                            // Address of the first byte of flash        (configured)
static U32            _StartAddrConf;                       // Address of the first byte used as storage (configured)
static U32            _NumBytes;                            // Number of bytes to be used as storage     (configured)
static U32            _StartAddrUsed;                       // Start address actually used (aligned to start of a sector)
static SECTOR_BLOCK   _aSectorBlockUsed[MAX_SECTOR_BLOCKS]; // Sector blocks actually used as storage.
static U8             _NumSectorBlocksUsed;                 // Number of sector blocks used as storage.
static U8             _FirstSectorIndex;                    // Index of the first physical sector used for storage.
static U8             _NumSectorsUsed;                      // Number of physical sectors used as storage.
#if FS_NOR_PHY_USE_OS
  static volatile U32 _Status;
#endif

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

#if FS_NOR_PHY_USE_OS

/**********************************************************
*
*       FLASH_IRQHandler
*
*  Function description
*    Handles the flash operation complete interrupt.
*/
void FLASH_IRQHandler(void);
void FLASH_IRQHandler(void) {
  OS_EnterInterrupt();          // Inform embOS that interrupt code is running.
  _Status  = FLASH_SR;          // Save the status to a static variable and check it in the task.
  FLASH_SR = 0                  // Clear the flags to prevent further interrupts.
           | (1uL << SR_EOP_BIT)
           | (1uL << SR_OPERR_BIT)
           | (1uL << SR_WRPERR_BIT)
           | (1uL << SR_PGAERR_BIT)
           | (1uL << SR_PGPERR_BIT)
           | (1uL << SR_PGSERR_BIT)
           ;
  FS_X_OS_Signal();             // Wake up the task.
  OS_LeaveInterrupt();          // Inform embOS that interrupt code is left.
}

#endif // FS_NOR_PHY_USE_OS

/*********************************************************************
*
*      _Lock
*
*  Function description
*    Locks access to flash control register.
*/
static void _Lock(void) {
  U32 TimeOut;

  FLASH_CR |= (1uL << CR_LOCK_BIT);
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((FLASH_CR & (1uL << CR_LOCK_BIT)) != 0u) {
      break;
    }
    if (--TimeOut == 0u) {
      break;
    }
  }
}

/*********************************************************************
*
*      _Unlock
*
*  Function description
*    Unlocks access to flash control register.
*/
static int _Unlock(void) {
  int IsLocked;
  U32 TimeOut;

  IsLocked = 0;
  if ((FLASH_CR & (1uL << CR_LOCK_BIT)) != 0u) {
    FLASH_KEYR = KEYR_KEY1;
    FLASH_KEYR = KEYR_KEY2;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((FLASH_CR & (1uL << CR_LOCK_BIT)) == 0u) {
        IsLocked = 1;
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
*      _InvalidateCache
*/
static void _InvalidateCache(void) {
  if (FLASH_ACR & (1uL << ACR_DCEN_BIT)) {    // Data cache enabled?
    FLASH_ACR &= ~(1uL << ACR_DCEN_BIT);      // Disable data cache in order to invalidate it.
    FLASH_ACR |=   1uL << ACR_DCRST_BIT;
    FLASH_ACR &= ~(1uL << ACR_DCRST_BIT);
    FLASH_ACR |=   1uL << ACR_DCEN_BIT;       // Re-enable data cache.
  }
}

/*********************************************************************
*
*      _CalcParallelism
*/
static U32 _CalcParallelism(U32 NumBytes) {
  switch (NumBytes) {
  default:
    // through
  case 1:
    return CR_PSIZE_8;
  case 2:
    return CR_PSIZE_16;
  case 4:
    return CR_PSIZE_32;
  }
}

/*********************************************************************
*
*      _EnableProgram
*
*  Function description
*    Starts a programming operation.
*/
static void _EnableProgram(U32 NumBytes) {
  U32 Parallelism;

#if FS_NOR_PHY_USE_OS
  _Status = 0;
#endif // FS_NOR_PHY_USE_OS
  Parallelism = _CalcParallelism(NumBytes);
  //
  // Clear all error flags.
  //
  FLASH_SR = 0
           | (1uL << SR_WRPERR_BIT)
           | (1uL << SR_PGAERR_BIT)
           | (1uL << SR_PGPERR_BIT)
           | (1uL << SR_PGSERR_BIT)
           ;
  FLASH_CR &= ~(CR_PSIZE_MASK << CR_PSIZE_BIT);
  FLASH_CR |= Parallelism << CR_PSIZE_BIT;
  __DSB();                // Make sure that the data is written to register.
  FLASH_CR |= 1uL         << CR_PG_BIT;
  __DSB();                // Make sure that the data is written to register.
}

/*********************************************************************
*
*      _DisbleProgram
*
*  Function description
*    Ends a programming operation.
*/
static void _DisableProgram(void) {
  FLASH_CR &= ~((1uL           << CR_PG_BIT) |
                (CR_PSIZE_MASK << CR_PSIZE_BIT));
}

/*********************************************************************
*
*      _StartErase
*
*  Function description
*    Starts the execution of an erase operation.
*/
static void _StartErase(unsigned SectorIndex) {
  U32 Parallelism;

  Parallelism = _CalcParallelism(FS_NOR_PHY_MAX_NUM_BYTES);
  //
  // Clear all error flags.
  //
  FLASH_SR = 0
           | (1uL << SR_WRPERR_BIT)
           | (1uL << SR_PGAERR_BIT)
           | (1uL << SR_PGPERR_BIT)
           | (1uL << SR_PGSERR_BIT)
           ;
  //
  // Set flash controller to sector erase mode and select sector to be erased.
  //
  FLASH_CR &= ~((CR_SNB_MASK   << CR_SNB_BIT) |
                (CR_PSIZE_MASK << CR_PSIZE_BIT));
  FLASH_CR |= 0
           | (1uL         << CR_SER_BIT)
           | (SectorIndex << CR_SNB_BIT)
           | (Parallelism << CR_PSIZE_BIT)
#if FS_NOR_PHY_USE_OS
           | (1uL << CR_ERRIE_BIT)            // Enable interrupts.
           | (1uL << CR_EOPIE_BIT)
#endif // FS_NOR_PHY_USE_OS
           ;
  //
  // Start erasing.
  //
  FLASH_CR |= 1uL << CR_STRT_BIT;
  __DSB();                // Make sure that the data is written to register.
}

/*********************************************************************
*
*      _CompleteErase
*
*  Function description
*    Ends the execution of an erase operation.
*/
static void _CompleteErase(void) {
  FLASH_CR &= ~((1uL           << CR_SER_BIT) |
                (CR_SNB_MASK   << CR_SNB_BIT) |
                (CR_PSIZE_MASK << CR_PSIZE_BIT));
}

/*********************************************************************
*
*      _WaitForEndOfOperation
*
*  Function description
*    Waits until a flash operation completes.
*/
static int _WaitForEndOfOperation(void) {
  U32 Status;
  int r;
  U32 TimeOut;

  r = 1;    // Set to indicate an error.
  //
  // Wait for the operation to complete
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
#if FS_NOR_PHY_USE_OS
    r = FS_OS_Wait(WAIT_TIMEOUT_MS);
    Status = _Status;
#else
    Status = FLASH_SR;
    if ((Status & (1uL << SR_BUSY_BIT)) == 0u) {
      break;
    }
    if (--TimeOut == 0u) {
      break;
    }
#endif // FS_NOR_PHY_USE_OS
  }
  //
  // OK, operation completed. Check the result.
  //
  if        (Status & (1uL << SR_WRPERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash operation failed. Flash is write-protected.\n"));
  } else if (Status & (1uL << SR_PGAERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash operation failed. Access not aligned.\n"));
  } else if (Status & (1uL << SR_PGPERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash operation failed. Invalid number of bytes.\n"));
  } else if (Status & (1uL << SR_PGSERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash operation failed. Invalid configuration.\n"));
  } else if (TimeOut == 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash operation failed (Timeout).\n"));
  } else {
    r = 0;
  }
  return r;
}

/*********************************************************************
*
*      _WriteOff8
*/
static int _WriteOff8(U32 Off, const U8 * pData, U32 NumBytes) {
  U8  * pWrite;
  U32   StartAddr;
  int   r;
  int   IsLocked;

  r         = 0;              // Set to indicate success.
  StartAddr = _StartAddrUsed + Off;
  pWrite    = SEGGER_ADDR2PTR(U8, StartAddr);
  IsLocked  = _Unlock();
  _EnableProgram(1);
  do {
    *pWrite++ = *pData++;
    __DSB();                  // Wait until the data is written. Without this the write operation may occasionally fail with PGPERR set.
    r = _WaitForEndOfOperation();
    if (r != 0) {
      r = 1;                  // Error, operation failed.
      break;
    }
  } while (--NumBytes != 0u);
  _DisableProgram();
  if (IsLocked != 0) {
    _Lock();
  }
  return r;
}

#if (FS_NOR_PHY_MAX_NUM_BYTES >= 2)

/*********************************************************************
*
*      _WriteOff16
*/
static int _WriteOff16(U32 Off, const U16 * pData, U32 NumItems) {
  U16 * pWrite;
  U32   StartAddr;
  int   r;
  int   IsLocked;

  r         = 0;              // Set to indicate success.
  StartAddr = _StartAddrUsed + Off;
  pWrite    = SEGGER_ADDR2PTR(U16, StartAddr);
  IsLocked  = _Unlock();
  _EnableProgram(2);
  do {
    *pWrite++ = *pData++;
    __DSB();                  // Wait until the data is written. Without this the write operation may occasionally fail with PGPERR set.
    r = _WaitForEndOfOperation();
    if (r != 0) {
      r = 1;                  // Error, operation failed.
      break;
    }
  } while (--NumItems != 0u);
  _DisableProgram();
  if (IsLocked != 0) {
    _Lock();
  }
  return r;
}

#endif // FS_NOR_PHY_MAX_NUM_BYTES >= 2

#if (FS_NOR_PHY_MAX_NUM_BYTES >= 4)

/*********************************************************************
*
*      _WriteOff32
*/
static int _WriteOff32(U32 Off, const U32 * pData, U32 NumItems) {
  U32 * pWrite;
  U32   StartAddr;
  int   r;
  int   IsLocked;

  r         = 0;              // Set to indicate success.
  StartAddr = _StartAddrUsed + Off;
  pWrite    = SEGGER_ADDR2PTR(U32, StartAddr);
  IsLocked  = _Unlock();
  _EnableProgram(4);
  do {
    *pWrite++ = *pData++;
    __DSB();                  // Wait until the data is written. Without this the write operation may occasionally fail with PGPERR set.
    r = _WaitForEndOfOperation();
    if (r != 0) {
      r = 1;                  // Error, operation failed.
      break;
    }
  } while (--NumItems != 0u);
  _DisableProgram();
  if (IsLocked != 0) {
    _Lock();
  }
  return r;
}

#endif // FS_NOR_PHY_MAX_NUM_BYTES >= 4

/*********************************************************************
*
*      _WriteOff
*/
static int _WriteOff(U32 Off, const U8 * pData, U32 NumBytes) {
  int r;

  r = 0;             // Set to indicate success.
#if (FS_NOR_PHY_MAX_NUM_BYTES == 2)
  if ((Off & 1u) == (SEGGER_PTR2ADDR(pData) & 1u)) {
    if (NumBytes != 0) {
      if ((SEGGER_PTR2ADDR(pData) & 1u) != 0) {
        //
        // Write the leading unaligned byte.
        //
        r = _WriteOff8(Off, pData, 1);
        if (r != 0) {
          NumBytes = 0;                                 // Error, could not write data.
        } else {
          ++pData;
          ++Off;
          --NumBytes;
        }
      }
    }
    if (NumBytes != 0) {
      U32 NumItems;

      NumItems = NumBytes >> 1;
      if (NumItems != 0) {
        //
        // Write entire words.
        //
        r = _WriteOff16(Off, SEGGER_PTR2PTR(const U16, pData), NumItems);
        if (r != 0) {
          NumBytes = 0;                                 // Error, could not write data.
        } else {
          pData    += NumItems << 1;
          Off      += NumItems << 1;
          NumBytes -= NumItems << 1;
        }
      }
    }
  }
#endif // FS_NOR_PHY_MAX_NUM_BYTES == 2
#if (FS_NOR_PHY_MAX_NUM_BYTES == 4)
  if ((Off & 3u) == (SEGGER_PTR2ADDR(pData) & 3u)) {
    if (NumBytes != 0) {
      if ((SEGGER_PTR2ADDR(pData) & 1u) != 0) {
        //
        // Write the leading unaligned byte.
        //
        r = _WriteOff8(Off, pData, 1);
        if (r != 0) {
          NumBytes = 0;                                 // Error, could not write data.
        } else {
          ++pData;
          ++Off;
          --NumBytes;
        }
      }
    }
    if (NumBytes != 0) {
      if ((SEGGER_PTR2ADDR(pData) & 3u) != 0) {
        //
        // Write the leading unaligned half-word.
        //
        r = _WriteOff16(Off, SEGGER_PTR2PTR(const U16, pData), 1);
        if (r != 0) {
          NumBytes = 0;                                 // Error, could not write data.
        } else {
          pData    += 2;
          Off      += 2;
          NumBytes -= 2;
        }
      }
    }
    if (NumBytes != 0) {
      U32 NumItems;

      NumItems = NumBytes >> 2;
      if (NumItems != 0) {
        //
        // Write entire words.
        //
        r = _WriteOff32(Off, SEGGER_PTR2PTR(const U32, pData), NumItems);
        if (r != 0) {
          NumBytes = 0;                                 // Error, could not write data.
        } else {
          pData    += NumItems << 2;
          Off      += NumItems << 2;
          NumBytes -= NumItems << 2;
        }
      }
    }
    if (NumBytes >= 2) {
      //
      // Write the trailing unaligned half-word.
      //
      r = _WriteOff16(Off, SEGGER_PTR2PTR(const U16, pData), 1);
      if (r != 0) {
        NumBytes = 0;                                 // Error, could not write data.
      } else {
        pData    += 2;
        Off      += 2;
        NumBytes += 2;
      }
    }
  }
#endif // FS_NOR_PHY_MAX_NUM_BYTES == 4
  if (NumBytes != 0) {
    r = _WriteOff8(Off, pData, NumBytes);
  }
  return r;
}

/*********************************************************************
*
*      _GetSectorOff
*/
static U32 _GetSectorOff(unsigned SectorIndex) {
  unsigned i;
  unsigned NumBlocks;
  unsigned NumSectors;
  U32      SectorSize;
  U32      Off;

  NumBlocks = _NumSectorBlocksUsed;
  Off = 0;
  for (i = 0; i < NumBlocks; i++) {
    NumSectors = _aSectorBlockUsed[i].NumSectors;
    SectorSize = _aSectorBlockUsed[i].SectorSize;
    if (SectorIndex < NumSectors) {
      NumSectors = SectorIndex;
    }
    Off += NumSectors * SectorSize;
    SectorIndex -= NumSectors;   // Number of remaining sectors
  }
  return Off;
}

/*********************************************************************
*
*      _GetSectorSize
*/
static U32 _GetSectorSize(unsigned SectorIndex) {
  unsigned i;
  unsigned NumBlocks;
  unsigned NumSectors;
  U32      SectorSize;

  NumBlocks = _NumSectorBlocksUsed;
  for (i = 0; i < NumBlocks; i++) {
    NumSectors = _aSectorBlockUsed[i].NumSectors;
    SectorSize = _aSectorBlockUsed[i].SectorSize;
    if (SectorIndex < NumSectors) {
      return SectorSize;
    }
    SectorIndex -= NumSectors;   // Number of remaining sectors
  }
  return 0;                      // Invalid sector index. Should not happen.
}

/*********************************************************************
*
*      _CalcStorageArea
*
*  Function description
*    Determines which physical sectors are used as storage.
*/
static int _CalcStorageArea(void) {
  U32            NumSectorBlocks;
  U8             NumSectorBlocksUsed;
  U32            NumBytesToSkip;
  U32            NumBytesRem;
  U32            NumSectorsUsed;
  U32            NumBytesSkipped;
  U32            i;
  U32            BaseAddr;
  U32            StartAddrUsed;
  U32            SectorIndex;
  U8             FirstSectorIndex;
  SECTOR_BLOCK * pSectorBlockUsed;

  //
  // Read physical sector block information and add it to the list of used blocks
  //
  NumSectorBlocks     = SEGGER_COUNTOF(_aSectorBlockDevice);
  NumSectorBlocksUsed = 0;
  NumSectorsUsed      = 0;
  NumBytesSkipped     = 0;
  BaseAddr            = _BaseAddr;
  NumBytesToSkip      = _StartAddrConf - BaseAddr;
  NumBytesRem         = _NumBytes;
  StartAddrUsed       = 0;
  pSectorBlockUsed    = _aSectorBlockUsed;
  SectorIndex         = 0;
  FirstSectorIndex    = 0;
  for (i = 0; i < NumSectorBlocks; i++) {
    U32 NumSectors;
    U32 SectorSize;

    NumSectors = _aSectorBlockDevice[i].NumSectors;
    SectorSize = _aSectorBlockDevice[i].SectorSize;
    //
    // Take care of bytes to skip before data area.
    //
    while (NumSectors && (NumBytesToSkip > 0)) {
      if (NumBytesToSkip > SectorSize) {
        NumBytesToSkip  -= SectorSize;
      } else {
        NumBytesToSkip   = 0;
      }
      NumBytesSkipped += SectorSize;
      NumSectors--;
      SectorIndex++;
    }
    if (NumSectors) {
      U32 NumSectorsRem;

      NumSectorsRem = NumBytesRem / SectorSize;
      if (NumSectors > NumSectorsRem) {
        NumSectors  = NumSectorsRem;
        NumBytesRem = 0;      // No more sectors after this to make sure that the sectors are adjacent!
      } else {
        NumBytesRem -= NumSectors * SectorSize;
      }
      //
      // Take care of bytes to skip after data area.
      //
      if (NumSectors) {
        if (NumSectorBlocksUsed == 0) {
          StartAddrUsed    = BaseAddr + NumBytesSkipped;      // Remember address of first sector used.
          FirstSectorIndex = SectorIndex;
        }
        pSectorBlockUsed->SectorSize = SectorSize;
        pSectorBlockUsed->NumSectors = NumSectors;
        ++NumSectorBlocksUsed;
        ++pSectorBlockUsed;
        NumSectorsUsed += NumSectors;
      }
    }
  }
  _NumSectorBlocksUsed = NumSectorBlocksUsed;
  _NumSectorsUsed      = NumSectorsUsed;
  _StartAddrUsed       = StartAddrUsed;
  _FirstSectorIndex    = FirstSectorIndex;
  //
  // Perform a sanity check.
  //
  if (NumSectorBlocksUsed == 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F2: Flash size to small for this configuration."));
    return 1;
  }
  return 0;
}

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
*   Function description
*     This routine writes data into any section of the flash. It does not
*     check if this section has been previously erased; this is in the
*     responsibility of the user program.
*     Data written into multiple sectors at a time can be handled by this
*     routine.
*
*   Parameters
*     Unit      Unit number of physical layer
*     Off       Byte offset to read from
*     pData     [IN]  Data to be written to flash
*     NumBytes  Number of bytes to write
*
*   Return value
*     ==0   OK, data has been written
*     !=    An error occurred
*/
static int _PHY_WriteOff(U8 Unit, U32 Off, const void * pData, U32 NumBytes) {
  int r;

  FS_USE_PARA(Unit);
  r = 0;              // OK, data written to flash.
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
*    Physical layer function. Reads data from the given byte offset of the flash.
*
*  Parameters
*    Unit       Unit number of physical layer
*    pData      [OUT] Data read from flash
*    Off        Byte offset to read from
*    NumBytes   Number of bytes to read
*
*  Return value
*    ==0    OK, data read from flash
*    !=0    An error occurred
*/
static int _PHY_ReadOff(U8 Unit, void * pData, U32 Off, U32 NumBytes) {
  U32 ReadAddr;
  U32 StartAddr;
  int r;

  FS_USE_PARA(Unit);
  StartAddr = _StartAddrUsed;
  ReadAddr  = StartAddr + Off;
  FS_MEMCPY(pData, (U8 *)ReadAddr, NumBytes);
  r = 0;      // OK, data read from flash.
  return r;
}

/*********************************************************************
*
*      _PHY_EraseSector
*
*  Function description
*    Physical layer function. Erases one physical sector.
*
*  Parameters
*    Unit           Unit number of physical layer
*    SectorIndex    Index of physical sector to erase
*
*  Return value
*    ==0    OK, sector is erased
*    !=0    An error occurred
*/
static int _PHY_EraseSector(U8 Unit, unsigned int SectorIndex) {
  int r;
  int IsLocked;

  FS_USE_PARA(Unit);
  ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
  SectorIndex += _FirstSectorIndex;
  (void)_WaitForEndOfOperation();
  IsLocked = _Unlock();
  _StartErase(SectorIndex);
  r = _WaitForEndOfOperation();
  _CompleteErase();
  _InvalidateCache();         // Invalidate the cache to ensure data consistency (See [2] page 13).
  if (IsLocked != 0) {
    _Lock();
  }
  return r;
}

/*********************************************************************
*
*       _PHY_GetSectorInfo
*
*  Function description
*    Physical layer function. Returns the byte offset and length in bytes of
*    the physical sector with the given index.
*
*  Parameters
*    Unit           Unit number of physical layer
*    SectorIndex    Index of physical sector to query. The index is relative to start of storage.
*    pOff           [OUT] Byte offset of physical sector
*    pLen           [OUT] Length of physical sector in bytes
*/
static void _PHY_GetSectorInfo(U8 Unit, unsigned int SectorIndex, U32 * pOff, U32 * pLen) {
  U32 SectorOff;
  U32 SectorSize;

  FS_USE_PARA(Unit);
  //
  // Fail if the SectorIndex is out of bounds or the device parameters are not set.
  //
  ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
  //
  // Compute result.
  //
  SectorSize = _GetSectorSize(SectorIndex);
  SectorOff  = _GetSectorOff(SectorIndex);
  if (pOff) {
    *pOff = SectorOff;
  }
  if (pLen) {
    *pLen = SectorSize;
  }
}

/*********************************************************************
*
*       _PHY_GetNumSectors
*
*  Function description
*    Physical layer function. Returns the number of logical sectors which can be stored to flash.
*
*  Parameters
*    Unit     Unit number of physical layer
*
*  Return value
*    <=0      An error occurred
*    > 0      Number of logical sectors which can be stored to NOR flash
*/
static int _PHY_GetNumSectors(U8 Unit) {
  int r;

  FS_USE_PARA(Unit);
  r = _NumSectorsUsed;
  return r;
}

/*********************************************************************
*
*       _PHY_Configure
*
*  Function description
*    Physical layer function. Configures the instance of the physical layer.
*
*  Parameters
*    Unit         Unit number of physical layer
*    BaseAddr     Address of the first byte in flash
*    StartAddr    Address of the first byte to be used as storage by the physical layer
*    NumBytes     Number of bytes to use as storage
*/
static void _PHY_Configure(U8 Unit, U32 BaseAddr, U32 StartAddr, U32 NumBytes) {
  FS_USE_PARA(Unit);
  _BaseAddr      = BaseAddr;
  _StartAddrConf = StartAddr;
  _NumBytes      = NumBytes;
}

/*********************************************************************
*
*       _PHY_OnSelectPhy
*
*  Function description
*    Physical layer function. Called right after selection of the physical layer.
*
*  Parameters
*    Unit    Unit number of physical layer
*/
static void _PHY_OnSelectPhy(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*      _PHY_Init
*
*  Function description
*    Identifies the MCU and computes the start address and the number of sectors used as storage.
*/
static int _PHY_Init(U8 Unit) {
  int r;

  FS_USE_PARA(Unit);
  r = _CalcStorageArea();
#if FS_NOR_PHY_USE_OS
  if (r == 0) {
    NVIC_SetPriority(FLASH_IRQn, FLASH_PRIO);
    NVIC_EnableIRQ(FLASH_IRQn);
  }
#endif // FS_NOR_PHY_USE_OS
  return r;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_ST_STM32F207 = {
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
