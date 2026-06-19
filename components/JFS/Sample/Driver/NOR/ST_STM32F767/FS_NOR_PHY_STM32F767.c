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

File    : FS_NOR_PHY_STM32F767.c
Purpose : NOR physical layer for the internal flash memory of ST STM32F767.
Literature:
  [1] RM0410 Reference manual STM32F76xxx and STM32F77xxx advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\RM0410_STM32F76xxx_STM32F77xxx_Rev2_DM00224583.pdf)
  [2] STM32F765xx STM32F767xx STM32F768Ax STM32F769xx Data sheet
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\DS11532_STM32F765xx_STM32F767xx_STM32F768Ax_STM32F769xx_Datasheet_Rev4.pdf)
  [2] STM32F76xxx STM32F77xxx Errata sheet
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\STM32F76xxx_STM32F77xxx_Errata_Rev4.pdf)
  [4]  UM1974 User manual STM32 Nucleo-144 board
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\EvalBoard\NUCLEO-F767ZI\UM1974_en.DM00244518.pdf)
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
#include <stm32f767xx.h>

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_PHY_MAX_NUM_BYTES
  #define FS_NOR_PHY_MAX_NUM_BYTES      4   // Maximum number of bytes that can be written at once.
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
#define FLASH_BASE_ADDR         0x40023C00
#define FLASH_ACR               (*(volatile U32*)(FLASH_BASE_ADDR + 0x00))
#define FLASH_KEYR              (*(volatile U32*)(FLASH_BASE_ADDR + 0x04))
#define FLASH_OPTKEYR           (*(volatile U32*)(FLASH_BASE_ADDR + 0x08))
#define FLASH_SR                (*(volatile U32*)(FLASH_BASE_ADDR + 0x0C))
#define FLASH_CR                (*(volatile U32*)(FLASH_BASE_ADDR + 0x10))
#define FLASH_OPTCR             (*(volatile U32*)(FLASH_BASE_ADDR + 0x14))

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
*       Misc. registers
*/
#define SYS_FSIZE               (*(volatile U16*)(0x1FF0F442))

/*********************************************************************
*
*       Flash control register
*/
#define CR_PG_BIT               0     // Start programming
#define CR_SER_BIT              1
#define CR_SNB_BIT              3
#define CR_SNB_MASK             0x1FuL
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
*       Flash status register
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
*       Flash unlock keys
*/
#define KEYR_KEY1               0x45670123
#define KEYR_KEY2               0xCDEF89AB

/*********************************************************************
*
*       Flash access control register
*/
#define ACR_ARTEN_BIT           9
#define ACR_ARTRST_BIT          11

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
#define CCR_DC_BIT              16
#define OPTR_DBANK_BIT          29
#define MAX_SECTOR_BLOCKS       6   // Number regions with the same sector size.
#define BANK_INDEX_BIT          4
#define WAIT_TIMEOUT_CYCLES     200000000uL   // This is the number of cycles required to create a delay of about 7 seconds at a CPU frequency of 216 MHz

/*********************************************************************
*
*       ASSERT_SECTOR_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)                                \
    if (SectorIndex >= _NumSectorsUsed) {                                             \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Invalid sector index.\n")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                            \
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
static const SECTOR_BLOCK _aSectorBlock2MB_DualBank[] = {
  {16  * 1024, 4},
  {64  * 1024, 1},
  {128 * 1024, 7},
  {16  * 1024, 4},
  {64  * 1024, 1},
  {128 * 1024, 7}
};

static const SECTOR_BLOCK _aSectorBlock2MB_SingleBank[] = {
  {32  * 1024, 4},
  {128 * 1024, 1},
  {256 * 1024, 7}
};

static const SECTOR_BLOCK _aSectorBlock1MB_DualBank[] = {
  {16  * 1024, 4},
  {64  * 1024, 1},
  {128 * 1024, 3},
  {16  * 1024, 4},
  {64  * 1024, 1},
  {128 * 1024, 3}
};

static const SECTOR_BLOCK _aSectorBlock1MB_SingleBank[] = {
  {32  * 1024, 4},
  {128 * 1024, 1},
  {256 * 1024, 3}
};

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static U32                  _BaseAddr;                            // Address of the first byte of flash        (configured)
static U32                  _StartAddrConf;                       // Address of the first byte used as storage (configured)
static U32                  _NumBytesConf;                        // Number of bytes to be used as storage     (configured)
static U32                  _StartAddrUsed;                       // Start address actually used (aligned to start of a sector)
static SECTOR_BLOCK         _aSectorBlockUsed[MAX_SECTOR_BLOCKS]; // Sector blocks actually used as storage.
static const SECTOR_BLOCK * _paSectorBlockDevice;                 // Sector blocks of the actual device.
static U32                  _NumSectorBlocksDevice;               // Number of sector blocks in the actual device.
static U8                   _NumSectorBlocksUsed;                 // Number of sector blocks used as storage.
static U8                   _FirstSectorIndex;                    // Index of the first physical sector used for storage.
static U8                   _NumSectorsUsed;                      // Number of physical sectors used as storage.
static U8                   _SectorsPerBank;                      // Number of sectors in a bank.

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
  if (FLASH_ACR & (1uL << ACR_ARTEN_BIT)) {    // Data cache enabled?
    FLASH_ACR &= ~(1uL << ACR_ARTEN_BIT);      // Disable data cache in order to invalidate it.
    FLASH_ACR |=   1uL << ACR_ARTRST_BIT;
    FLASH_ACR &= ~(1uL << ACR_ARTRST_BIT);
    FLASH_ACR |=   1uL << ACR_ARTEN_BIT;       // Re-enable data cache.
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
*   Function description
*     Ends the execution of an erase operation.
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
*   Function description
*     Waits until a flash operation completes.
*
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
    Status = FLASH_SR;
    if ((Status & (1uL << SR_BUSY_BIT)) == 0u) {
      break;
    }
    if (--TimeOut == 0u) {
      break;
    }
  }
  //
  // OK, operation completed. Check the result.
  //
  if (Status & (1uL << SR_WRPERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash operation failed (Flash is write-protected).\n"));
  } else if (Status & (1uL << SR_PGAERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash operation failed (Access not aligned).\n"));
  } else if (Status & (1uL << SR_PGPERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash operation failed (Invalid number of bytes).\n"));
  } else if (Status & (1uL << SR_PGSERR_BIT)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash operation failed (Invalid configuration).\n"));
  } else if (TimeOut == 0u) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash operation failed (Timeout).\n"));
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
  NumSectorBlocks     = _NumSectorBlocksDevice;
  NumSectorBlocksUsed = 0;
  NumSectorsUsed      = 0;
  NumBytesSkipped     = 0;
  BaseAddr            = _BaseAddr;
  NumBytesToSkip      = _StartAddrConf - BaseAddr;
  NumBytesRem         = _NumBytesConf;
  StartAddrUsed       = 0;
  pSectorBlockUsed    = _aSectorBlockUsed;
  SectorIndex         = 0;
  FirstSectorIndex    = 0;
  for (i = 0; i < NumSectorBlocks; i++) {
    U32 NumSectors;
    U32 SectorSize;

    NumSectors = _paSectorBlockDevice[i].NumSectors;
    SectorSize = _paSectorBlockDevice[i].SectorSize;
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
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32F767: Flash size to small for this configuration."));
    return 1;
  }
  return 0;
}

/*********************************************************************
*
*      _CalcBRSI
*
*  Function description
*    Calculates the sector index relative to a bank.
*/
static U32 _CalcBRSI(U32 SectorIndex) {
  SectorIndex += _FirstSectorIndex;
  if (SectorIndex >= _SectorsPerBank) {
    SectorIndex -= _SectorsPerBank;
    SectorIndex |= 1u << BANK_INDEX_BIT;
  }
  return SectorIndex;
}

/*********************************************************************
*
*      _CalcNumSectors
*
*  Function description
*    Calculates the total number of sectors in device.
*/
static unsigned _CalcNumSectors(const SECTOR_BLOCK * pSectorBlock, unsigned NumSectorBlocks) {
  unsigned NumSectors;
  unsigned i;

  NumSectors = 0;
  for (i = 0; i < NumSectorBlocks; ++i) {
    NumSectors += pSectorBlock->NumSectors;
    ++pSectorBlock;
  }
  return NumSectors;
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
      // Use MPU to disable the data cache on the memory region assigned to internal flash.
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

  FS_USE_PARA(Unit);
  r = 0;                  // OK, data written to flash.
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
  U32 ReadAddr;
  U32 StartAddr;

  FS_USE_PARA(Unit);
  StartAddr = _StartAddrUsed;
  ReadAddr  = StartAddr + Off;
  FS_MEMCPY(pData, (U8 *)ReadAddr, NumBytes);
  return 0;               // OK, data read from flash.
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
  int IsLocked;

  FS_USE_PARA(Unit);
  ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
  SectorIndex = _CalcBRSI(SectorIndex);
  (void)_WaitForEndOfOperation();
  IsLocked = _Unlock();
  _StartErase(SectorIndex);
  r = _WaitForEndOfOperation();
  _CompleteErase();
  _InvalidateCache();         // Invalidate the cache to ensure data consistency.
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
  //
  // Disable the cache on the flash region used as storage.
  //
  _DisableDCache();
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
  int IsDualBank;
  U32 NumSectors;

  FS_USE_PARA(Unit);
  //
  // Determine the characteristics of the internal flash memory.
  //
  FlashSize   = SYS_FSIZE;                        // The flash size register stores the number of Kbytes.
  FlashSize <<= 10;                               // Calculate the total number of bytes in the internal flash memory.
  //
  // Check for dual-bank configuration.
  //
  IsDualBank = 0;
  if ((FLASH_OPTCR & (1uL << OPTR_DBANK_BIT)) == 0u) {
    IsDualBank = 1;
  }
  //
  // Select the correct flash organization.
  //
  if (IsDualBank != 0) {
    if (FlashSize > 0x000100000) {
      _paSectorBlockDevice   = _aSectorBlock2MB_DualBank;
      _NumSectorBlocksDevice = SEGGER_COUNTOF(_aSectorBlock2MB_DualBank);
    } else {
      _paSectorBlockDevice   = _aSectorBlock1MB_DualBank;
      _NumSectorBlocksDevice = SEGGER_COUNTOF(_aSectorBlock1MB_DualBank);
    }
  } else {
    if (FlashSize > 0x000100000) {
      _paSectorBlockDevice   = _aSectorBlock2MB_SingleBank;
      _NumSectorBlocksDevice = SEGGER_COUNTOF(_aSectorBlock2MB_SingleBank);
    } else {
      _paSectorBlockDevice   = _aSectorBlock1MB_SingleBank;
      _NumSectorBlocksDevice = SEGGER_COUNTOF(_aSectorBlock1MB_SingleBank);
    }
  }
  //
  // Calculate the number of sectors in a bank.
  //
  NumSectors = _CalcNumSectors(_paSectorBlockDevice, _NumSectorBlocksDevice);
  if (IsDualBank != 0) {
    NumSectors >>= 1;
  }
  _SectorsPerBank = NumSectors;
  //
  // Calculate which sectors are used as storage.
  //
  r = _CalcStorageArea();
  return r;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_NOR_PHY_STM32F767
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_STM32F767 = {
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
