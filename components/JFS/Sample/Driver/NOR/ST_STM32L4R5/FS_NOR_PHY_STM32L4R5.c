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

File    : FS_NOR_PHY_STM32L4R5.c
Purpose : NOR physical layer for the internal flash memory of ST STM32L4R5.
Literature:
  [1] RM0432 Reference manual STM32L4+ Series advanced Arm-based 32-bit MCUs
  [2] STM32L4Rxxx STM32L4Sxxx Errata sheet
Additional information:
  This physical layer expects that the parameters of the internal flash
  memory such as number of wait states are configured by the application
  which is typically the case.

  The characteristics of the internal flash memory demand that the
  data is written 8 bytes at a time and that the write operations
  are aligned to an 8 byte boundary. In addition, it is not possible
  to rewrite a previously programmed flash memory location without
  an erase operation in between. Because of these constraints the
  NOR driver has to be compiled with the following configuration:
    FS_NOR_CAN_REWRITE           = 0
    FS_NOR_LINE_SIZE             = 8
    FS_NOR_OPTIMIZE_HEADER_WRITE = 1
  More information about these configuration defines can be found
  in the emFile manual.
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
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       FLASH registers
*/
#define FLASH_BASE_ADDR         0x40022000
#define FLASH_ACR               (*(volatile U32*)(FLASH_BASE_ADDR + 0x00))
#define FLASH_KEYR              (*(volatile U32*)(FLASH_BASE_ADDR + 0x08))
#define FLASH_SR                (*(volatile U32*)(FLASH_BASE_ADDR + 0x10))
#define FLASH_CR                (*(volatile U32*)(FLASH_BASE_ADDR + 0x14))
#define FLASH_OPTR              (*(volatile U32*)(FLASH_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Misc. registers
*/
#define FLASH_SIZE              (*(volatile U16*)(0x1FFF75E0))

/*********************************************************************
*
*       Flash control register
*/
#define CR_PG_BIT               0
#define CR_PER_BIT              1
#define CR_PNB_BIT              3
#define CR_BKER_BIT             11
#define CR_START_BIT            16
#define CR_EOPIE_BIT            24
#define CR_ERRIE_BIT            25
#define CR_LOCK_BIT             31

/*********************************************************************
*
*       Flash status register
*/
#define SR_EOP_BIT              0
#define SR_OPERR_BIT            1
#define SR_PROGERR_BIT          3
#define SR_WRPERR_BIT           4
#define SR_PGAERR_BIT           5
#define SR_SIZERR_BIT           6
#define SR_PGSERR_BIT           7
#define SR_MISERR_BIT           8
#define SR_FASTERR_BIT          9
#define SR_RDERR_BIT            14
#define SR_OPTVERR_BIT          15
#define SR_BSY_BIT              16

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
#define ACR_DCEN_BIT            10
#define ACR_DCRST_BIT           12

/*********************************************************************
*
*       Misc. defines
*/
#define LD_BYTES_PER_SECTOR_DUAL_BANK       12   // Size of a physical sector in dual-bank mode (power of 2 value).
#define LD_BYTES_PER_SECTOR_SINGLE_BANK     13   // Size of a physical sector in single-bank mode (power of 2 value).
#define BYTES_PER_LINE                      8
#define OPTR_DBANK_BIT                      22
#define OPTR_DB1M_BIT                       21

/*********************************************************************
*
*       ASSERT_UNIT_NO_IS_IN_RANGE
*/
#if (FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL)
  #define ASSERT_DATA_IS_ALIGNED(Off, NumBytes)                                    \
    if (   ((Off & (BYTES_PER_LINE - 1u)) != 0)                                    \
        || ((NumBytes & (BYTES_PER_LINE - 1u)) != 0)) {                            \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_STM32L4R5: Data is not aligned.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                         \
    }
#else
  #define ASSERT_DATA_IS_ALIGNED(Off, NumBytes)
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
static U8  _ldBytesPerSector;   // Number of bytes in a physical sector as a power of 2 value.
static U32 _BytesPerSector;     // Number of bytes in a physical sector.
static U32 _BaseAddr;           // Address of the first byte in the internal flash.
static U32 _StartAddrConf;      // Address of the first byte to be used as storage as configured by the application.
static U32 _NumBytesConf;       // Number of bytes to be used as storage as configured by the application.
static U32 _StartAddrUsed;      // Address of the first byte to be used as storage aligned to a physical sector boundary.
static U32 _NumBytesUsed;       // Number of bytes to be used as storage as configured by the application.
static U16 _NumSectorsUsed;     // Number of physical sectors used as storage.
static U8  _IsDualBank;         // Set to 1 if the internal flash memory works in dual-bank mode.

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _LoadU32LE
*
*  Function description
*    Reads a 32 bit little endian from a char array.
*
*  Parameters
*    pBuffer    Pointer to a char array.
*
*  Return value
*    The value as U32 data type.
*/
static U32 _LoadU32LE(const U8 *pBuffer) {
  U32 r;
  r   = (U32)pBuffer[3] & 0x000000FFuL;
  r <<= 8;
  r  += (U32)pBuffer[2] & 0x000000FFuL;
  r <<= 8;
  r  += (U32)pBuffer[1] & 0x000000FFuL;
  r <<= 8;
  r  += (U32)pBuffer[0] & 0x000000FFuL;
  return r;
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
  ldBytesPerSector = _ldBytesPerSector;
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
static void _WaitForReady(void) {
  //
  // Wait for the flash memory to become ready for the operation.
  //
  for(;;) {
    if ((FLASH_SR & (1uL << SR_BSY_BIT)) == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*      _UnlockIfRequired
*/
static int _UnlockIfRequired(void) {
  int IsLocked;

  IsLocked = 0;
  if ((FLASH_CR & (1uL << CR_LOCK_BIT)) != 0u) {
    IsLocked = 1;
    FLASH_KEYR = KEYR_KEY1;
    FLASH_KEYR = KEYR_KEY2;
    //
    // Wait for the flash to unlock.
    //
    for (;;) {
      if ((FLASH_CR & (1uL << CR_LOCK_BIT)) == 0u) {
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
static void _Lock(void) {
  FLASH_CR |= 1uL << CR_LOCK_BIT;
  //
  // Wait for the flash to lock.
  //
  for (;;) {
    if ((FLASH_CR & (1uL << CR_LOCK_BIT)) != 0u) {
      break;
    }
  }
}

/*********************************************************************
*
*      _ClearErrors
*/
static void _ClearErrors(void) {
  FLASH_SR = 0u
           | (1uL << SR_EOP_BIT)
           | (1uL << SR_OPERR_BIT)
           | (1uL << SR_PROGERR_BIT)
           | (1uL << SR_WRPERR_BIT)
           | (1uL << SR_PGAERR_BIT)
           | (1uL << SR_SIZERR_BIT)
           | (1uL << SR_PGSERR_BIT)
           | (1uL << SR_MISERR_BIT)
           | (1uL << SR_FASTERR_BIT)
           | (1uL << SR_RDERR_BIT)
           | (1uL << SR_OPTVERR_BIT)
           ;
}

/*********************************************************************
*
*      _WaitForEndOfOperation
*/
static int _WaitForEndOfOperation(void) {
  int r;
  U32 Status;

  r = 0;                  // Set to indicate success.
  for (;;) {
    Status = FLASH_SR;
    if ((Status & (1uL << SR_BSY_BIT)) == 0u) {
      if ((Status & (1uL << SR_PROGERR_BIT)) != 0u) {
        r = 1;            // Error, program operation failed.
        break;
      }
      if ((Status & (1uL << SR_WRPERR_BIT)) != 0u) {
        r = 1;            // Error, write protection error.
        break;
      }
      if ((Status & (1uL << SR_PGAERR_BIT)) != 0u) {
        r = 1;            // Error, program alignment error.
        break;
      }
      if ((Status & (1uL << SR_SIZERR_BIT)) != 0u) {
        r = 1;            // Error, size error.
        break;
      }
      if ((Status & (1uL << SR_PGSERR_BIT)) != 0u) {
        r = 1;            // Error, program sequence error.
        break;
      }
      if ((Status & (1uL << SR_OPERR_BIT)) != 0u) {
        r = 1;            // Error, operation failed.
        break;
      }
      if ((Status & (1uL << SR_EOP_BIT)) != 0u) {
        break;            // Error, operation was successful.
      }
    }
  }
  return r;
}

/*********************************************************************
*
*      _GetBankIndex
*/
static unsigned _GetBankIndex(unsigned SectorIndex) {
  unsigned BankIndex;

  BankIndex = 0;
  if (_IsDualBank != 0u) {
    if (SectorIndex >= (_NumSectors >> 1u)) {
      BankIndex = 1;
    }
  }
  return BankIndex;
}

/*********************************************************************
*
*      _GetPageIndex
*/
static unsigned _GetPageIndex(unsigned SectorIndex) {
  unsigned PageIndex;

  PageIndex = SectorIndex;
  if (_IsDualBank != 0u) {
    if (SectorIndex >= (_NumSectors >> 1u)) {
      PageIndex = SectorIndex - (_NumSectors >> 1u);
    }
  }
  return PageIndex;
}

/*********************************************************************
*
*      _InvalidateDCache
*/
static void _InvalidateDCache(void) {
  if ((FLASH_ACR & (1uL << ACR_DCEN_BIT)) != 0u) {
    //
    // The data cache can be invalidated only if disabled.
    //
    FLASH_ACR &= ~(1uL << ACR_DCEN_BIT);
    //
    // Reset the data cache.
    //
    FLASH_ACR |=   1uL << ACR_DCRST_BIT;
    FLASH_ACR &= ~(1uL << ACR_DCRST_BIT);
    //
    // Re-enable the data cache.
    //
    FLASH_ACR |=   1uL << ACR_DCEN_BIT;
  }
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
*    SectorIndex is relative to the beginning of the internal flash memory.
*/
static int _EraseSector(unsigned SectorIndex) {
  int      IsLocked;
  int      r;
  unsigned BankIndex;
  unsigned PageIndex;

  _WaitForReady();
  //
  // If required, unlock the access to control register.
  //
  IsLocked = _UnlockIfRequired();
  //
  // According to [1] all the errors have to be cleared before starting
  // the operation otherwise the operation will fail.
  //
  _ClearErrors();
  //
  // Set up and start the erase operation.
  //
  BankIndex = _GetBankIndex(SectorIndex);
  PageIndex = _GetPageIndex(SectorIndex);
  FLASH_CR = 0
           | (BankIndex << CR_BKER_BIT)
           | (PageIndex << CR_PNB_BIT)
           | (1uL       << CR_EOPIE_BIT)        // Signal the end of operation.
           | (1uL       << CR_ERRIE_BIT)        // Signal an error condition.
           | (1uL       << CR_PER_BIT)
           | (1uL       << CR_START_BIT)
           ;
  //
  // If required, lock again the access to the control register.
  //
  if (IsLocked != 0) {
    _Lock();
  }
  //
  // Wait for the end of operation.
  //
  r = _WaitForEndOfOperation();
  //
  // Invalidate the data cache.
  //
  _InvalidateDCache();
  return r;
}

/*********************************************************************
*
*      _Write
*
*  Function description
*    Writes data to internal flash memory.
*
*  Parameters
*    pAddr      Address to write to.
*    pData      Data to be written.
*    NumItems   Number of words (32-bit items) to be written.
*
*  Return value
*    ==0    OK, data was written.
*    !=0    An error occurred.
*
*  Additional information
*    NumItems has to be an even value because we have to write 8 bytes at a time.
*    pData does not have to be 32-bit aligned.
*/
static int _Write(U32 * pAddr, const U8 * pData, unsigned NumItems) {
  int         IsLocked;
  int         IsDCacheEnabled;
  int         r;
  const U32 * pData32;
  U32         Data32;

  _WaitForReady();
  //
  // If required, unlock the access to control register.
  //
  IsLocked = _UnlockIfRequired();
  //
  // According to [1] all the errors have to be cleared before starting
  // the operation otherwise the operation will fail.
  //
  _ClearErrors();
  //
  // According to section "2.2.2" in [2] the data cache must be disabled
  // before any write operation.
  //
  IsDCacheEnabled = 0;
  if ((FLASH_ACR & (1uL << ACR_DCEN_BIT)) != 0u) {
    FLASH_ACR &= ~(1uL << ACR_DCEN_BIT);
    IsDCacheEnabled = 1;
  }
  //
  // Set up and start the programming operation.
  //
  FLASH_CR = 0
           | (1uL << CR_EOPIE_BIT)          // Signal the end of operation.
           | (1uL << CR_ERRIE_BIT)          // Signal an error condition.
           | (1uL << CR_PG_BIT)             // Request a write operation.
           ;
  //
  // Write the data line by line.
  //
  if ((SEGGER_PTR2ADDR(pData) & 3) == 0) {  // Is the data 32-bit aligned?
    pData32 = SEGGER_PTR2PTR(U32, pData);
    for (;;) {
      *pAddr++ = *pData32++;
      *pAddr++ = *pData32++;
      NumItems -= 2;
      r = _WaitForEndOfOperation();
      if (r != 0) {
        break;                              // Error, could not write data.
      }
      if (NumItems == 0) {
        break;                              // OK, all data was written.
      }
    }
  } else {
    for (;;) {
      Data32 = _LoadU32LE(pData);
      *pAddr++ = Data32;
      pData += 4;
      Data32 = _LoadU32LE(pData);
      *pAddr++ = Data32;
      pData += 4;
      NumItems -= 2;
      r = _WaitForEndOfOperation();
      if (r != 0) {
        break;                              // Error, could not write data.
      }
      if (NumItems == 0) {
        break;                              // OK, all data was written.
      }
    }
  }
  //
  // Disable the programming operation.
  //
  FLASH_CR = 0;
  //
  // Re-enable the data cache if necessary.
  //
  if (IsDCacheEnabled != 0) {
    //
    // Reset the data cache.
    //
    FLASH_ACR |=   1uL << ACR_DCRST_BIT;
    FLASH_ACR &= ~(1uL << ACR_DCRST_BIT);
    //
    // Re-enable the data cache.
    //
    FLASH_ACR |=   1uL << ACR_DCEN_BIT;
  }
  //
  // If required, lock again the access to the control register.
  //
  if (IsLocked != 0) {
    _Lock();
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
  int      r;
  U32      Addr;
  unsigned NumItems;

  ASSERT_DATA_IS_ALIGNED(Off, NumBytes);
  FS_USE_PARA(Unit);
  r        = 0;                         // Set to indicate success.
  Addr     = _StartAddrUsed + Off;
  NumItems = NumBytes >> 2;             // We write 4 bytes at a time.
  if (NumItems != 0u) {
    r = _Write(SEGGER_ADDR2PTR(U32, Addr), pData, NumItems);
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
  const void * p;
  U32          Addr;

  FS_USE_PARA(Unit);
  Addr = _StartAddrUsed + Off;
  p = SEGGER_ADDR2PTR(const void, Addr);
  FS_MEMCPY(pData, p, NumBytes);
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
  int r;
  U32 StartSector;

  FS_USE_PARA(Unit);
  StartSector  = (_StartAddrUsed - _BaseAddr) >> _ldBytesPerSector;
  SectorIndex += StartSector;
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
    *pOff = SectorIndex << _ldBytesPerSector;
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
  FlashSize   = FLASH_SIZE;                       // The FLASH_SIZE register stores the number of Kbytes.
  FlashSize <<= 10;                               // Calculate the total number of bytes in the internal flash memory.
  //
  // Check for dual-bank configuration.
  // The dual-bank mode is specified differently for devices
  // with an internal flash memory larger smaller than 2 Mbyte.
  //
  _IsDualBank = 0;
  if (FlashSize > 0x100000) {
    if ((FLASH_OPTR & (1uL << OPTR_DBANK_BIT)) != 0u) {
      _IsDualBank = 1;
    }
  } else {
    if ((FLASH_OPTR & (1uL << OPTR_DB1M_BIT)) != 0u) {
      _IsDualBank = 1;
    }
  }
  _ldBytesPerSector = LD_BYTES_PER_SECTOR_SINGLE_BANK;
  if (_IsDualBank != 0) {
    _ldBytesPerSector = LD_BYTES_PER_SECTOR_DUAL_BANK;
  }
  _BytesPerSector = 1uL << _ldBytesPerSector;
  //
  // Calculate the total number of physical sectors
  // and the number of sectors as well as the address
  // of the first byte to be used as storage.
  //
  _NumSectors = FlashSize >> _ldBytesPerSector;
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
*       FS_NOR_PHY_STM32L4R5
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_STM32L4R5 = {
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
