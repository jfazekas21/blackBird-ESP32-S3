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

File    : FS_NOR_PHY_EnergyMicro_EFM32GG.c
Purpose : Physical layer for the internal flash of EnergyMicro EFM32GG MCU.
Literature:
  [1] "EFM32GG Reference Manual "Giant Gecko" Series"
    (\\FILESERVER\Techinfo\Company\EnergyMicro\GiantGecko\d0053_EFM32GG_TRM_2012-04-24_Rev0.96.pdf)
  [2] "EFM32GG990 Errata, Chip rev C F1024/F512"
    (\\FILESERVER\Techinfo\Company\EnergyMicro\GiantGecko\d0103_efm32gg990_errata.pdf)
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
#define RUN_FROM_FLASH      1     // Set this to 1 if the code is located on one half of the flash
                                  // and the storage on the other half
                                  // in order to save some RAM space.

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
#define MSC_BASE_ADDR             0x400C0000uL
#define MSC_CTRL                  (*(volatile U32 *)(MSC_BASE_ADDR + 0x00))   // Memory System Control Register
#define MSC_READCTRL              (*(volatile U32 *)(MSC_BASE_ADDR + 0x04))   // Read Control Register
#define MSC_WRITECTRL             (*(volatile U32 *)(MSC_BASE_ADDR + 0x08))   // Write Control Register
#define MSC_WRITECMD              (*(volatile U32 *)(MSC_BASE_ADDR + 0x0C))   // Write Command Register
#define MSC_ADDRB                 (*(volatile U32 *)(MSC_BASE_ADDR + 0x10))   // Page Erase/Write Address Buffer
#define MSC_WDATA                 (*(volatile U32 *)(MSC_BASE_ADDR + 0x18))   // Write Data Register
#define MSC_STATUS                (*(volatile U32 *)(MSC_BASE_ADDR + 0x1C))   // Status Register
#define MSC_IF                    (*(volatile U32 *)(MSC_BASE_ADDR + 0x2C))   // Interrupt Flag Register
#define MSC_IFS                   (*(volatile U32 *)(MSC_BASE_ADDR + 0x30))   // Interrupt Flag Set Register
#define MSC_IFC                   (*(volatile U32 *)(MSC_BASE_ADDR + 0x34))   // Interrupt Flag Clear Register
#define MSC_IEN                   (*(volatile U32 *)(MSC_BASE_ADDR + 0x38))   // Interrupt Enable Register
#define MSC_LOCK                  (*(volatile U32 *)(MSC_BASE_ADDR + 0x3C))   // Configuration Lock Register
#define MSC_CMD                   (*(volatile U32 *)(MSC_BASE_ADDR + 0x40))   // Command Register
#define MSC_CACHEHITS             (*(volatile U32 *)(MSC_BASE_ADDR + 0x44))   // Cache Hits Performance Counter
#define MSC_CACHEMISSES           (*(volatile U32 *)(MSC_BASE_ADDR + 0x48))   // Cache Misses Performance Counter
#define MSC_TIMEBASE              (*(volatile U32 *)(MSC_BASE_ADDR + 0x50))   // Flash Write and Erase Timebase
#define MSC_MASSLOCK              (*(volatile U32 *)(MSC_BASE_ADDR + 0x54))   // Mass Erase Lock Register

/*********************************************************************
*
*       Lock register
*/
#define LOCK_LOCKED_BIT           0
#define LOCK_KEY                  0x1B71uL

/*********************************************************************
*
*       Status register
*/
#define STATUS_BUSY_BIT           0
#define STATUS_LOCKED_BIT         1
#define STATUS_INVADDR_BIT        2
#define STATUS_WDATAREADY_BIT     3
#define STATUS_WORDTIMEOUT_BIT    4
#define STATUS_ERASEABORTED_BIT   5

/*********************************************************************
*
*       Write command register
*/
#define WRITECMD_LADDRIM_BIT      0
#define WRITECMD_ERASEPAGE_BIT    1
#define WRITECMD_WRITEEND_BIT     2
#define WRITECMD_WRITEONCE_BIT    3
#define WRITECMD_WRITETRIG_BIT    4

/*********************************************************************
*
*       Write control register
*/
#define WRITECTRL_WREN_BIT        0
#define WRITECTRL_WDOUBLE_BIT     2
#define WRITECTRL_RWWEN_BIT       5

/*********************************************************************
*
*       Error status
*/
#define STATUS_OK                 0
#define STATUS_ERASE_ERROR        1
#define STATUS_INVALID_ADDR       2
#define STATUS_LOCK_ERROR         3

/*********************************************************************
*
*       Other defines
*/
#define MEM_INFO_PAGE_SIZE        (*(volatile U8  *)0x0FE081E7)           // Flash page size in bytes coded as 2 ^((MEM_INFO_PAGE_SIZE + 10) & 0xFF)
#define MEM_INFO_FLASH            (*(volatile U16 *)0x0FE081F8)           // Flash size, kbyte count as unsigned integer
#define CMU_AUXHFRCOCTRL          (*(volatile U32 *)0x400C8014)           // AUXHFRCO Control Register
#define PART_FAMILY               (*(volatile U8  *)0x0FE081FE)           // EFM32 part family number
#define PART_FAMILY_GIANT_GECKO   72
#define AUXHFRCOCTRL_BAND_BIT     8
#define AUXHFRCOCTRL_BAND_MASK    0x7uL

/*********************************************************************
*
*       RAM execution
*/
#if RUN_FROM_FLASH
  #define RAM_FUNC
#else
  #define RAM_FUNC __ramfunc
#endif

/*********************************************************************
*
*       ASSERT_SECTOR_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)                            \
    if (((SectorIndex) << _pInst->SectorSizeShift) >= _pInst->NumBytes) {         \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Invalid sector index.")); \
      FS_X_Panic(FS_ERRCODE_INVALID_PARA);                                        \
    }
#else
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)
#endif

/*********************************************************************
*
*       ASSERT_DATA_IS_ALIGNED
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_DATA_IS_ALIGNED(v)                                           \
    if ((v) & 7) {                                                            \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Data not aligned.")); \
      FS_X_Panic(FS_ERRCODE_INVALID_PARA);                                    \
    }
#else
  #define ASSERT_DATA_IS_ALIGNED(v)
#endif

/*********************************************************************
*
*      Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*      NOR_EFM32GG_INST
*/
typedef struct NOR_EFM32GG_INST {
  U32 BaseAddr;               // Address of the first byte of flash        (configured)
  U32 StartAddrConf;          // Address of the first byte used as storage (configured)
  U32 NumBytes;               // Number of bytes to be used as storage     (configured)
  U32 StartAddrUsed;          // Start address actually used (aligned to start of a sector)
  U8  SectorSizeShift;        // Size of the sector (page) as power of 1.
} NOR_EFM32GG_INST;

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static NOR_EFM32GG_INST * _pInst;

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*      _Lock
*
*  Function description
*    Locks access to MSC_CTRL, MSC_READCTRL, MSC_WRITECMD, and MSC_TIMEBASE registers.
*/
static void _Lock(void) {
  if ((MSC_LOCK & (1uL << LOCK_LOCKED_BIT)) == 0) {
    MSC_LOCK = 0;           // Any other value than the lock key locks the access to registers.
  }
}

/*********************************************************************
*
*      _Unlock
*
*  Function description
*    Unlocks access to MSC_CTRL, MSC_READCTRL, MSC_WRITECMD, and MSC_TIMEBASE registers.
*/
static void _Unlock(void) {
  if (MSC_LOCK & (1uL << LOCK_LOCKED_BIT)) {
    MSC_LOCK = LOCK_KEY;
  }
}

/*********************************************************************
*
*       _GetFlashSize
*
*  Function description
*    Returns the total size of the internal flash in bytes.
*/
static U32 _GetFlashSize(void) {
  U32 FlashSize;

  FlashSize = MEM_INFO_FLASH;   // Device information page
  FlashSize <<= 10;             // Convert KB size into size in bytes
  return FlashSize;
}

/*********************************************************************
*
*       _GetPageSizeShift
*
*  Function description
*    Returns the size of a flash page. A page is the smallest erasable unit.
*/
static U8 _GetPageSizeShift(void) {
  U8  Family;
  U32 PageSizeShift;

  Family = PART_FAMILY;   // There is a byte in the device information page which gives information about the device family
  switch (Family) {
  case PART_FAMILY_GIANT_GECKO: PageSizeShift = 12; break;                          // Giant Gecko (4 KB pages)
  default:                      PageSizeShift = (MEM_INFO_PAGE_SIZE + 10); break;   // Default
  }
  return (U8)PageSizeShift;
}

/*********************************************************************
*
*      _ReportError
*/
static void _ReportError(int ErrorCode) {
  switch (ErrorCode) {
  case STATUS_OK:
    break;
  case STATUS_ERASE_ERROR:
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Flash operation failed. Erase aborted.\n"));
    break;
  case STATUS_INVALID_ADDR:
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Flash operation failed. Invalid address.\n"));
    break;
  case STATUS_LOCK_ERROR:
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Flash operation failed. Access locked.\n"));
    break;
  default:
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Flash operation failed with error %d.\n", ErrorCode));
    break;
  }
}

/*********************************************************************
*
*      _WaitForEndOfOperation
*
*  Function description
*    Waits for the flash operation to complete.
*
*  Return value
*    ==0    OK, operation successfull
*    !=0    An error occurred
*/
static RAM_FUNC int _WaitForEndOfOperation(void) {
  U32 Status;
  int r;

  //
  // Wait for the operation to complete
  //
  do {
    Status = MSC_STATUS;
  } while (Status & (1uL << STATUS_BUSY_BIT));
  //
  // OK, operation completed. Check the result.
  //
  if        (Status & (1uL << STATUS_ERASEABORTED_BIT)) {
    r = STATUS_ERASE_ERROR;
  } else if (Status & (1uL << STATUS_INVADDR_BIT)) {
    r = STATUS_INVALID_ADDR;
  } else if (Status & (1uL << STATUS_LOCKED_BIT)) {
    r = STATUS_LOCK_ERROR;
  } else {
    r = STATUS_OK;
  }
  return r;
}

/*********************************************************************
*
*      _EraseSector
*
*  Function description
*    Executes a page erase operation.
*/
static RAM_FUNC int _EraseSector(unsigned SectorIndex) {
  U32 SectorAddr;
  U32 NumBytes;
  U8  SectorSizeShift;
  int r;

  SectorSizeShift  = _pInst->SectorSizeShift;
  NumBytes         = SectorIndex << SectorSizeShift;
  SectorAddr       = _pInst->StartAddrUsed + NumBytes;
  MSC_ADDRB    = SectorAddr;
  MSC_WRITECMD = 1uL << WRITECMD_LADDRIM_BIT;       // Transfer the address from ADDRB register to the internal address register of the flash.
  MSC_WRITECMD = 1uL << WRITECMD_ERASEPAGE_BIT;
  r = _WaitForEndOfOperation();
  return r;
}

/*********************************************************************
*
*      _WriteOff
*/
static RAM_FUNC int _WriteOff(U32 Off, const void * pData, U32 NumBytes) {
  U32 * pSrc;
  U32   DestAddr;
  int   r;
  U32   NumItems;
  U32   Status;

  r        = 0;                 // OK, data written to flash.
  DestAddr = _pInst->StartAddrUsed + Off;
  pSrc     = (U32 *)pData;
  NumItems = NumBytes >> 2;     // We have to write 4 bytes at a time.

  do {
    //
    // Transfer addr from ADDRB register to flash internal address register.
    // Address will be incremented automatically during programming, within the same page.
    //
    MSC_ADDRB    = DestAddr;
    MSC_WRITECMD = 1uL << WRITECMD_LADDRIM_BIT;
    do {
      //
      // We have to write 2 words at a time when the
      // WDOUBLE flag is set in the MSC_WRITECTRL register.
      //
      MSC_WDATA = *pSrc++;
      //
      // Wait until we are allowed to write to data register.
      //
      while (1) {
        Status = MSC_STATUS;
        if (Status & (1uL << STATUS_WDATAREADY_BIT)) {
          break;
        }
      }
      MSC_WDATA  = *pSrc++;
      NumItems  -= 2;
      //
      // Start the write operation.
      //
      MSC_WRITECMD = 1uL << WRITECMD_WRITEONCE_BIT;
      //
      // Wait until flash controller is ready again.
      //
      r = _WaitForEndOfOperation();
      if (r) {
        break;      // Error, could not write to flash
      }
    } while (NumItems);
  } while (NumItems);
  return r;
}

/*********************************************************************
*
*      _Init
*
*  Function description
*    Identifies the MCU and computes the start address and the number of sectors used as storage.
*/
static void _Init(void) {
  U32 NumBytesToSkip;
  U32 NumBytesRem;
  U32 NumBytesSkipped;
  U32 BaseAddr;
  U32 StartAddrUsed;
  U32 NumSectors;
  U32 SectorSize;
  U8  SectorSizeShift;
  U32 FlashSize;
  U32 v;

  NumBytesSkipped = 0;
  BaseAddr        = _pInst->BaseAddr;
  NumBytesToSkip  = _pInst->StartAddrConf - BaseAddr;
  NumBytesRem     = _pInst->NumBytes;
  StartAddrUsed   = 0;
  FlashSize       = _GetFlashSize();
  SectorSizeShift = _GetPageSizeShift();
  NumSectors      = FlashSize >> SectorSizeShift;
  SectorSize      = 1uL << SectorSizeShift;
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
  }
  StartAddrUsed = BaseAddr + NumBytesSkipped;
  if (NumSectors) {
    U32 NumSectorsRem;

    NumSectorsRem = NumBytesRem >> SectorSizeShift;
    if (NumSectors > NumSectorsRem) {
      NumSectors  = NumSectorsRem;
      NumBytesRem = 0;                                      // No more sectors after this to make sure that the sectors are adjacent!
    } else {
      NumBytesRem -= NumSectors << SectorSizeShift;
    }
  }
  //
  // Perform a sanity check.
  //
  if (NumSectors == 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_EFM32GG: Flash size to small for this configuration."));
  }
  _pInst->StartAddrUsed    = StartAddrUsed;
  _pInst->SectorSizeShift  = SectorSizeShift;
  //
  // Configure the flash controller.
  //
  _Unlock();
  MSC_WRITECTRL = 0                            // Enable write/erase functionality of the MSC & enable 2-word writing (flash is written 2-word wise)
                | (1uL << WRITECTRL_WREN_BIT)
                | (1uL << WRITECTRL_WDOUBLE_BIT)
                | (1uL << WRITECTRL_RWWEN_BIT)
                ;
  //
  // For some devices make sure that the MSC_TIMEBASE register is correctly set
  // According to the documentation we need to set the rounded up number of AUX cycles - 1 in 1.1us or 5.5us to this register
  // After boot up, AUX is at 14 MHz so AUX cycles in 1.1us would be 15.4 (rounded up 16) and the reset value of TIMEBASE is 0x10,
  // so it seems that not AUX cycles - 1 is stored but AUX cycles.
  //
  v = (CMU_AUXHFRCOCTRL >> AUXHFRCOCTRL_BAND_BIT) & AUXHFRCOCTRL_BAND_MASK;
  switch(v) {
  default:
  case 0: v = 16;             break; // AUX = 14 MHz, 16 cycles in 1.1us
  case 1: v = 13;             break; // AUX = 11 MHz, 13 cycles in 1.1us
  case 2: v = 39 | (1 << 16); break; // AUX =  7 MHz, 39 cycles in 5.5us Note: EM recommends to use cycles / 5.5us for this frequency
  case 3: v = 6  | (1 << 16); break; // AUX =  1 MHz,  6 cycles in 5.5us Note: EM recommends to use cycles / 5.5us for this frequency
  case 6: v = 31;             break; // AUX = 28 MHz, 31 cycles in 1.1us
  case 7: v = 24;             break; // AUX = 21 MHz, 24 cycles in 1.1us
  }
  MSC_TIMEBASE = v;
  _Lock();
}

/*********************************************************************
*
*      _AllocIfRequired
*
*  Function description
*    Allocates an instance of the physical layer if necessary.
*/
static NOR_EFM32GG_INST * _AllocIfRequired(void) {
  if (_pInst == NULL) {
    _pInst = (NOR_EFM32GG_INST *)FS_AllocZeroed(sizeof(NOR_EFM32GG_INST));
  }
  return _pInst;
}

/*********************************************************************
*
*      Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*      _PHY_WriteOff
*
*  Function description
*    This routine writes data into any section of the flash. It does not
*    check if this section has been previously erased; this is in the
*    responsibility of the user program.
*    Data written into multiple sectors at a time can be handled by this
*    routine.
*
*  Parameters
*    Unit       Unit number of physical layer
*    Off        Byte offset to read from
*    pData      [IN]  Data to be written to flash
*               [OUT] ---
*    NumBytes   Number of bytes to write
*
*  Return value
*    ==0    OK, data has been written
*    !=0    An error occurred
*/
static int _PHY_WriteOff(U8 Unit, U32 Off, const void * pData, U32 NumBytes) {
  int r;

  FS_USE_PARA(Unit);
  r = 1;                // Set to indicate an error.
  if (_pInst) {
    r = 0;              // OK, data written to flash.
    if (NumBytes) {
      ASSERT_DATA_IS_ALIGNED(Off | NumBytes);
      _Unlock();
      r = _WriteOff(Off, pData, NumBytes);
      _Lock();
      if (r) {
        //lint -save -e522
        _ReportError(r);
        //lint -restore
      }
    }
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
*    pData      [IN]  ---
*               [OUT] Data read from flash
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
  r = 1;        // Set to indicate an error.
  if (_pInst) {
    StartAddr = _pInst->StartAddrUsed;
    ReadAddr  = StartAddr + Off;
    FS_MEMCPY(pData, (U8 *)ReadAddr, NumBytes);
    r = 0;      // OK, data read from flash.
  }
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

  FS_USE_PARA(Unit);
  r = 1;
  if (_pInst) {
     ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
    _Unlock();
    r = _EraseSector(SectorIndex);
    _Lock();
    if (r) {
      //lint -save -e522
      _ReportError(r);
      //lint -restore
    }
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
*    pOff           [IN]  ---
*                   [OUT] Byte offset of physical sector
*    pLen           [IN]  ---
*                   [OUT] Length of physical sector in bytes
*/
static void _PHY_GetSectorInfo(U8 Unit, unsigned int SectorIndex, U32 * pOff, U32 * pLen) {
  U32 SectorOff;
  U32 SectorSize;

  FS_USE_PARA(Unit);
  SectorOff  = 0;
  SectorSize = 0;
  if (_pInst) {
    //
    // Fail if the SectorIndex is out of bounds or the device parameters are not set.
    //
    ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
    //
    // Compute result.
    //
    SectorSize = 1uL << _pInst->SectorSizeShift;
    SectorOff  = SectorSize * SectorIndex;
  }
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
  r = 0;
  if (_pInst) {
    r = (int)(_pInst->NumBytes >> _pInst->SectorSizeShift);
  }
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
  _AllocIfRequired();
  if (_pInst) {
    _pInst->BaseAddr      = BaseAddr;
    _pInst->StartAddrConf = StartAddr;
    _pInst->NumBytes      = NumBytes;
    _Init();
  }
}

/*********************************************************************
*
*       _PHY_OnSelectPhy
*
*  Function description
*    Physical layer function. Called right after selection of the physical layer.
*
*  Parameters
*    Unit     Unit number of physical layer
*/
static void _PHY_OnSelectPhy(U8 Unit) {
  FS_USE_PARA(Unit);
  _AllocIfRequired();
}

/*********************************************************************
*
*       _PHY_DeInit
*
*  Function description
*    Physical layer function. Frees up the memory allocated for the instance.
*
*  Parameters
*    Unit     Unit number of physical layer
*/
static void _PHY_DeInit(U8 Unit) {
  FS_USE_PARA(Unit);
#if FS_SUPPORT_DEINIT
  FS_Free((void *)_pInst);
  _pInst = NULL;
#endif
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_EnergyMicro_EFM32GG = {
  _PHY_WriteOff,
  _PHY_ReadOff,
  _PHY_EraseSector,
  _PHY_GetSectorInfo,
  _PHY_GetNumSectors,
  _PHY_Configure,
  _PHY_OnSelectPhy,
  _PHY_DeInit,
  NULL,
  NULL
};

/*************************** End of file ****************************/
