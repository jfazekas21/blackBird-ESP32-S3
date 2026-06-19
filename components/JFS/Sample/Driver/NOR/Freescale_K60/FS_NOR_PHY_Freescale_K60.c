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

File    : FS_NOR_PHY_Freescale_K60.c
Purpose : Low level driver for the internal flash of Freescale Kinetis K60 MCU.
Literature:
  [1] K60 Sub-Family Reference Manual
    (\\fileserver\Techinfo\Company\NXP\MCU\Kinetis_K-series\K60P144M150SF3RM.pdf)
  [2] Mask Set Errata for Mask 0M33Z
    (\\fileserver\Techinfo\Company\NXP\MCU\Errata\KINETIS_0M33Z.pdf)
Additional information
  The file system will have to be built with the configuration defines
  set in FS_Conf.h as below because the internal flash does not have
  support for incremental write operations and it can write only
  64-bits at a time:
    FS_NOR_CAN_REWRITE=0
    FS_NOR_LINE_SIZE=8
    FS_NOR_OPTIMIZE_HEADER_WRITE=1
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
*       Flash memory module
*/
#define FTFE_BASE_ADDR            0x40020000uL
#define FTFE_FSTAT                (*(volatile U8*)(FTFE_BASE_ADDR + 0x00))
#define FTFE_FCNFG                (*(volatile U8*)(FTFE_BASE_ADDR + 0x01))
#define FTFE_FSEC                 (*(volatile U8*)(FTFE_BASE_ADDR + 0x02))
#define FTFE_FOPT                 (*(volatile U8*)(FTFE_BASE_ADDR + 0x03))
#define FTFE_FCCOB3               (*(volatile U8*)(FTFE_BASE_ADDR + 0x04))
#define FTFE_FCCOB2               (*(volatile U8*)(FTFE_BASE_ADDR + 0x05))
#define FTFE_FCCOB1               (*(volatile U8*)(FTFE_BASE_ADDR + 0x06))
#define FTFE_FCCOB0               (*(volatile U8*)(FTFE_BASE_ADDR + 0x07))
#define FTFE_FCCOB7               (*(volatile U8*)(FTFE_BASE_ADDR + 0x08))
#define FTFE_FCCOB6               (*(volatile U8*)(FTFE_BASE_ADDR + 0x09))
#define FTFE_FCCOB5               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0A))
#define FTFE_FCCOB4               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0B))
#define FTFE_FCCOBB               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0C))
#define FTFE_FCCOBA               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0D))
#define FTFE_FCCOB9               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0E))
#define FTFE_FCCOB8               (*(volatile U8*)(FTFE_BASE_ADDR + 0x0F))
#define FTFE_FPROT3               (*(volatile U8*)(FTFE_BASE_ADDR + 0x10))
#define FTFE_FPROT2               (*(volatile U8*)(FTFE_BASE_ADDR + 0x11))
#define FTFE_FPROT1               (*(volatile U8*)(FTFE_BASE_ADDR + 0x12))
#define FTFE_FPROT0               (*(volatile U8*)(FTFE_BASE_ADDR + 0x13))
#define FTFE_FEPROT               (*(volatile U8*)(FTFE_BASE_ADDR + 0x16))
#define FTFE_FDPROT               (*(volatile U8*)(FTFE_BASE_ADDR + 0x17))

/*********************************************************************
*
*       Flash memory controller
*/
#define FMC_BASE_ADDR             0x4001F000uL
#define FMC_PFAPR                 (*(volatile U32*)(FMC_BASE_ADDR + 0x00))
#define FMC_PFB0CR                (*(volatile U32*)(FMC_BASE_ADDR + 0x04))
#define FMC_PFB1CR                (*(volatile U32*)(FMC_BASE_ADDR + 0x08))

/*********************************************************************
*
*       System integration module
*/
#define SIM_BASE_ADDR             0x40048000
#define SIM_SDID                  (*(volatile U32 *)(SIM_BASE_ADDR + 0x24))   // Manual says 0x40047024 but this is not correct

/*********************************************************************
*
*       FSTAT register flags
*/
#define FSTAT_CCIF                7
#define FSTAT_RDCOLERR            6
#define FSTAT_ACCERR              5
#define FSTAT_FPVIOL              4
#define FSTAT_MGSTAT0             0

/*********************************************************************
*
*       Flash commands
*/
#define CMD_PROGRAM_LONGWORD      0x06 // Program 4 bytes in a program flash block or a data flash block.
#define CMD_PROGRAM_PHRASE        0x07 // Program 8 bytes in a program flash block
#define CMD_ERASE_FLASH_SECTOR    0x09 // Erase all bytes in a program flash or data flash sector.

/*********************************************************************
*
*       ASSERT_SECTOR_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)                        \
    if (SectorIndex >= _pInst->NumSectors) {                                  \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K90: Invalid sector index.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                    \
    }
#else
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex)
#endif

/*********************************************************************
*
*       ASSERT_DEVICE_IS_SET
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_DEVICE_IS_SET()                                              \
    if (_pInst->pDevice == NULL) {                                            \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K90: Device not set."));       \
      FS_X_PANIC(FS_ERRCODE_UNKNOWN_DEVICE);                                  \
    }
#else
  #define ASSERT_DEVICE_IS_SET()
#endif

/*********************************************************************
*
*       ASSERT_DATA_IS_ALIGNED
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_DATA_IS_ALIGNED(Data)                                        \
    if ((Off | NumBytes) & ((1 << _pInst->pDevice->ldLineSize) - 1)) {        \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K90: Data not aligned."));     \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                    \
    }
#else
  #define ASSERT_DATA_IS_ALIGNED(Data)
#endif

/*********************************************************************
*
*      Types
*
**********************************************************************
*/
typedef struct {
  U8  DeviceID;               // Bits 9:7 of SIM_SDID register
  U8  ldLineSize;             // Number of bytes in a flash line as power of 2
  U16 SectorSize;             // Size of a physical sector in bytes
  U32 SizeofFlash;            // Flash capacity in bytes
} NOR_K60_DEVICE;

typedef struct {
  U32 BaseAddr;               // Address of the first byte of flash        (configured)
  U32 StartAddrConf;          // Address of the first byte used as storage (configured)
  U32 NumBytes;               // Number of bytes to be used as storage     (configured)
  U32 StartAddrUsed;          // Start addr. actually used (aligned to start of a sector)
  U16 NumSectors;             // Number of physical sectors used as storage
  const NOR_K60_DEVICE * pDevice;
} NOR_K60_INST;

/*********************************************************************
*
*      Static const
*
**********************************************************************
*/

//
// Later kinetis devices only allow programming of 64-bit items
// We check the system identification register to identify the Kinetis device
// SIM_SDID[9:7] == 3 indicate that only 64-bit programming is allowed
// SIM_SDID[9:7] 000 ->  max.  128 KB Flash,    max.  32 KB FlexNVM,   1 KB sector size,   program longword
// SIM_SDID[9:7] 001 ->  max.  256 KB Flash,    max.  32 KB FlexNVM,   2 KB sector size,   program longword
// SIM_SDID[9:7] 010 ->  max.  256 KB Flash,    max. 256 KB FlexNVM,   2 KB sector size,   program longword
// SIM_SDID[9:7] 010 ->  max.  512 KB Flash,    max.   0 KB FlexNVM,   2 KB sector size,   program longword
// SIM_SDID[9:7] 011 ->  max.  512 KB Flash,    max. 512 KB FlexNVM,   4 KB sector size,   program phrase
// SIM_SDID[9:7] 011 ->  max. 1024 KB Flash,    max.   0 KB FlexNVM,   4 KB sector size,   program phrase
//
static const NOR_K60_DEVICE _aDevice[] =  {
  {0, 2, 1 * 1024,   128 * 1024},
  {1, 2, 2 * 1024,   256 * 1024},
  {2, 2, 2 * 1024,   512 * 1024},
  {3, 3, 4 * 1024,  1024 * 1024}
};

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static NOR_K60_INST * _pInst;

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*      _InitFMC
*
*  Function description
*    Initializes the flash memory controller.
*
*  Additional information
*    It is not allowed to access the flash while accessing the registers.
*    This is the reason why this routine should run in RAM.
*/
#if   defined (__ICCARM__)
  #define RAMFUNC       __ramfunc
#elif defined (__SES_ARM)
  #define RAMFUNC       __attribute__ ((section(".fast")))
#else
  #define RAMFUNC
  #warning "The function will not work reliably if not run from RAM!"
#endif
static RAMFUNC void _InitFMC(void) {
  //
  // Disable caching and prefetching for devices with a flash size larger than 512KB.
  // The cache is not working properly on these devices as documented in [2] (e2647, e2448 and e2671).
  //
  FMC_PFB0CR &= ~0x1FuL;
  FMC_PFB1CR &= ~0x1FuL;
}

/*********************************************************************
*
*      _Init
*
*   Function description
*     Identifies the MCU and computes the start address and the number of sectors used as storage.
*/
static void _Init(void) {
  const NOR_K60_DEVICE * pDevice;
  U16 NumSectors;
  U32 SectorSize;
  I32 NumBytesToSkip;
  I32 NumBytesRem;
  U32 NumBytesSkipped;
  U32 SizeofFlash;
  U32 i;
  U32 StartAddrUsed;
  U32 NumEntries;
  U32 BaseAddr;

  //
  // If not manually configured try to determine the physical sector size and the line length by reading the chip identification register.
  //
  if (_pInst->pDevice == NULL) {
    U32 DeviceID;

    //
    // Read the chip identification from the SIM_SDID register
    //
    DeviceID = (SIM_SDID >> 7) & 3;
    FS_DEBUG_LOG((FS_MTYPE_DRIVER, "NOR_K60: Found DeviceID: 0x%02x.\n", DeviceID));
    pDevice    = _aDevice;
    NumEntries = SEGGER_COUNTOF(_aDevice);
    for (i = 0; i < NumEntries; ++i) {
      if (pDevice->DeviceID == DeviceID) {
        _pInst->pDevice = pDevice;
        break;
      }
      ++pDevice;
    }
    if (i == NumEntries) {
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Could not identify device."));
      FS_X_PANIC(FS_ERRCODE_UNKNOWN_DEVICE);
    }
  } else {
    FS_DEBUG_LOG((FS_MTYPE_DRIVER, "NOR_K60: Device is selected manually.\n"));
  }
  //
  // OK, the device is identified. Determine which phyiscal sectors are used as storage.
  //
  NumBytesSkipped = 0;
  BaseAddr        = _pInst->BaseAddr;
  NumBytesToSkip  = _pInst->StartAddrConf - BaseAddr;
  NumBytesRem     = _pInst->NumBytes;
  SectorSize      = _pInst->pDevice->SectorSize;
  SizeofFlash     = _pInst->pDevice->SizeofFlash;
  NumSectors      = SizeofFlash / SectorSize;
  //
  // Take care of bytes to skip before storage area.
  //
  while (NumSectors && (NumBytesToSkip > 0)) {
    NumBytesToSkip  -= SectorSize;
    NumBytesSkipped += SectorSize;
    NumSectors--;
  }
  StartAddrUsed = BaseAddr + NumBytesSkipped;
  if (NumSectors) {
    U16 NumSectorsRem;

    NumSectorsRem = (U16)((U32)NumBytesRem / SectorSize);
    if (NumSectors > NumSectorsRem) {
      NumSectors  = NumSectorsRem;
      NumBytesRem = 0;      // No more sectors after this to make sure that sectors are adjacent!
    } else {
      NumBytesRem -= NumSectors * SectorSize;
    }
  }
  if (NumSectors == 0) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Flash size to small for this configuration. 0 bytes available."));
  }
  _pInst->StartAddrUsed = StartAddrUsed;
  _pInst->NumSectors    = NumSectors;
  _InitFMC();
}

/*********************************************************************
*
*      _AllocIfRequired
*
*   Function description
*     Allocates an instance of the physical layer if necessary.
*
*/
static NOR_K60_INST * _AllocIfRequired(void) {
  if (_pInst == NULL) {
     _pInst = (NOR_K60_INST *)FS_AllocZeroed(sizeof(NOR_K60_INST));
  }
  return _pInst;
}

/*********************************************************************
*
*      _StartOperation
*
*  Function description
*    Starts a flash operation.
*/
static RAMFUNC void _StartOperation(void) {
  //
  // Clear all error flags, since if one of them is set, the flash controller does not accept any commands.
  //
  FTFE_FSTAT = (1 << FSTAT_RDCOLERR)
             | (1 << FSTAT_ACCERR)
             | (1 << FSTAT_FPVIOL)
             ;
  //
  // Clear CCIF bit in FSTAT by writing a 1 to it, in order to start programming.
  // Setting the other bits to 0 has no effect, since the error flags are only cleared by writing 1 to them
  //
  FTFE_FSTAT = 1 << FSTAT_CCIF;
}

/*********************************************************************
*
*      _WaitForEndOfOperation
*
*  Function description
*    Waits until a flash operation completes.
*
*  Return value
*    Status reported by the flash controller
*/
static RAMFUNC U8 _WaitForEndOfOperation(void) {
  U8 Status;

  //
  // Wait for the operation to complete
  //
  do {
    Status = FTFE_FSTAT;
  } while((Status & (1 << FSTAT_CCIF)) == 0);
  return Status;
}

/*********************************************************************
*
*      _ExecOperation
*
*  Function description
*    Executes a flash command.
*/
static int _ExecOperation(void) {
  U8  Status;
  int r;

  r = 1;    // Set to indicate an error.
  FS_NOR_DI();
  _StartOperation();
  Status = _WaitForEndOfOperation();
  FS_NOR_EI();
  //
  // OK, operation completed. Check the result.
  //
  if (Status & (1 << FSTAT_RDCOLERR)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Flash operation failed. Read collision."));
  } else if (Status & (1 << FSTAT_ACCERR)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Flash operation failed. Invalid command."));
  } else if (Status & (1 << FSTAT_FPVIOL)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Flash operation failed. Flash is write-protected."));
  } else if (Status & (1 << FSTAT_MGSTAT0)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_K60: Flash operation failed. Memory controller error."));
  } else {
    r = 0;
  }
  return r;
}

/*********************************************************************
*
*      Global functions
*
**********************************************************************
*/

/*********************************************************************
*
*      _WriteOff
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
*               [OUT] ---
*     NumBytes  Number of bytes to write
*
*   Return value
*     ==0   OK, data has been written
*     !=    An error occurred
*/
static int _WriteOff(U8 Unit, U32 Off, const void * pData, U32 NumBytes) {
  U32   WriteAddr;
  U32   StartAddr;
  int   r;
  U32   Data32;
  U32 * p;
  U8    ldLineSize;
  U32   NumItems;
  U8    Cmd;

  FS_USE_PARA(Unit);
  r = 1;                // Set to indicate an error.
  if (_pInst) {
    if (NumBytes) {
      ASSERT_DEVICE_IS_SET();
      ASSERT_DATA_IS_ALIGNED(Off | NumBytes);
      StartAddr   = _pInst->StartAddrUsed;
      WriteAddr   = StartAddr + Off;
      p           = (U32 *)pData;
      ldLineSize  = _pInst->pDevice->ldLineSize;
      if (ldLineSize == 3) {
        Cmd = CMD_PROGRAM_PHRASE;
      } else {
        Cmd = CMD_PROGRAM_LONGWORD;
      }
      NumItems = NumBytes >> ldLineSize;
      if (NumItems) {
        do {
          Data32 = *p++;
          Data32 = ~(*(U32 *)WriteAddr) | Data32;               // Re-programming of existing 0s to 0 is not allowed.
          FTFE_FCCOB0 = Cmd;
          FTFE_FCCOB1 = (U8)(WriteAddr >> 16);
          FTFE_FCCOB2 = (U8)(WriteAddr >>  8);
          FTFE_FCCOB3 = (U8)(WriteAddr >>  0);
          FTFE_FCCOB4 = (U8)(Data32 >> 24);
          FTFE_FCCOB5 = (U8)(Data32 >> 16);
          FTFE_FCCOB6 = (U8)(Data32 >>  8);
          FTFE_FCCOB7 = (U8)(Data32 >>  0);
          WriteAddr += 4;
          NumBytes  -= 4;
          if (ldLineSize == 3) {
            Data32 = *p++;
            Data32 = ~(*(U32 *)WriteAddr) | Data32;             // Re-programming of existing 0s to 0 is not allowed.
            FTFE_FCCOB8 = (U8)(Data32 >> 24);
            FTFE_FCCOB9 = (U8)(Data32 >> 16);
            FTFE_FCCOBA = (U8)(Data32 >>  8);
            FTFE_FCCOBB = (U8)(Data32 >>  0);
            WriteAddr += 4;
            NumBytes  -= 4;
          }
          r = _ExecOperation();
          if (r) {
            return 1;       // Error, operation failed.
          }
        } while (--NumItems);
      }
    }
    r = 0;        // OK, data written to flash.
  }
  return r;
}

/*********************************************************************
*
*       _ReadOff
*
*   Function description
*     Physical layer function. Reads data from the given byte offset of the flash.
*
*   Parameters
*     Unit      Unit number of physical layer
*     pData     [IN]  ---
*               [OUT] Data read from flash
*     Off       Byte offset to read from
*     NumBytes  Number of bytes to read
*
*   Return value
*     ==0   OK, data read from flash
*     !=0   An error occurred
*/
static int _ReadOff(U8 Unit, void * pData, U32 Off, U32 NumBytes) {
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
*      _EraseSector
*
*   Function description
*     Physical layer function. Erases one physical sector.
*
*   Parameters
*     Unit          Unit number of physical layer
*     SectorIndex   Index of physical sector to erase
*
*   Return value
*     ==0   OK, sector is erased
*     !=0   An error occurred
*/
static int _EraseSector(U8 Unit, unsigned int SectorIndex) {
  int r;
  U32 StartAddr;
  U32 SectorSize;
  U32 SectorOff;

  FS_USE_PARA(Unit);
  r = 1;
  if (_pInst) {
    ASSERT_SECTOR_INDEX_IS_IN_RANGE(SectorIndex);
    ASSERT_DEVICE_IS_SET();
    SectorSize  = _pInst->pDevice->SectorSize;
    StartAddr   = _pInst->StartAddrUsed;
    SectorOff   = SectorSize * SectorIndex + StartAddr;
    FTFE_FCCOB0 = CMD_ERASE_FLASH_SECTOR;
    FTFE_FCCOB1 = (U8)(SectorOff >> 16);
    FTFE_FCCOB2 = (U8)(SectorOff >>  8);
    FTFE_FCCOB3 = (U8)(SectorOff >>  0);
    r = _ExecOperation();
  }
  return r;
}

/*********************************************************************
*
*       _GetSectorInfo
*
*   Function description
*     Physical layer function. Returns the byte offset and length in bytes of
*     the physical sector with the given index.
*
*   Parameters
*     Unit          Unit number of physical layer
*     SectorIndex   Index of physical sector to query. The index is relative to start of storage.
*     pOff          [IN]  ---
*                   [OUT] Byte offset of physical sector
*     pLen          [IN]  ---
*                   [OUT] Lengt of physical sector in bytes
*/
static void _GetSectorInfo(U8 Unit, unsigned int SectorIndex, U32 * pOff, U32 * pLen) {
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
    ASSERT_DEVICE_IS_SET();
    //
    // Compute result.
    //
    SectorSize = _pInst->pDevice->SectorSize;
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
*       _GetNumSectors
*
*   Function description
*     Physical layer function. Returns the number of logical sectors which can be stored to flash.
*
*   Parameters
*     Unit    Unit number of physical layer
*
*   Return value
*     <=0     An error occurred
*     > 0     Number of logical sectors which can be stored to NOR flash
*/
static int _GetNumSectors(U8 Unit) {
  int r;

  FS_USE_PARA(Unit);
  r = 0;
  if (_pInst) {
    r =_pInst->NumSectors;
  }
  return r;
}

/*********************************************************************
*
*       _Configure
*
*   Function description
*     Physical layer function. Configures the instance of the physical layer.
*
*   Parameters
*     Unit        Unit number of physical layer
*     BaseAddr    Address of the first byte in flash
*     StartAddr   Address of the first byte to be used as storage by the physical layer
*     NumBytes    Number of bytes to use as storage
*/
static void _Configure(U8 Unit, U32 BaseAddr, U32 StartAddr, U32 NumBytes) {
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
*       _OnSelectPhy
*
*   Function description
*     Physical layer function. Called right after selection of the physical layer.
*
*   Parameters
*     Unit    Unit number of physical layer
*/
static void _OnSelectPhy(U8 Unit) {
  FS_USE_PARA(Unit);
  _AllocIfRequired();
}

/*********************************************************************
*
*       _DeInit
*
*   Function description
*     Physical layer function. Frees up the memory allocated for the instance.
*
*   Parameters
*     Unit    Unit number of physical layer
*/
static void _DeInit(U8 Unit) {
  FS_USE_PARA(Unit);
#if FS_SUPPORT_DEINIT
  FS_Free((void *)_pInst);
  _pInst = NULL;
#endif
}

/*********************************************************************
*
*      Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_Freescale_K60 = {
  _WriteOff,
  _ReadOff,
  _EraseSector,
  _GetSectorInfo,
  _GetNumSectors,
  _Configure,
  _OnSelectPhy,
  _DeInit,
  NULL,
  NULL
};

/*************************** End of file ****************************/
