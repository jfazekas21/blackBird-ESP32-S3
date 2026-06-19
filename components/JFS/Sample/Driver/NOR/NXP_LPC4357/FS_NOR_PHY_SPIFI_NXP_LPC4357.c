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

File    : FS_NOR_PHY_SPIFI_NXP_LPC4357.c
Purpose : Low-level flash driver for NXP SPIFI interface.
Literature:
  [1] \\fileserver\Techinfo\Company\NXP\MCU\LPC43xx\UserManual_LPC43xx_UM10503_Rev1.50_121203.pdf
  [2] \\fileserver\Techinfo\Company\NXP\MCU\SPIFI_FlashProgrammingLib
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
*      Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_NUM_PHY_SECTORS
  #define FS_NOR_NUM_PHY_SECTORS    32            // Number of physical sectors in the NOR flash (if 0 the NOR flash is autodetected)
#endif

#ifndef   BYTES_PER_PHY_SECTOR
  #define BYTES_PER_PHY_SECTOR      0x10000       // Number of bytes in a physical sector (if 0 the NOR flash is autodetected)
#endif

#ifndef   BYTES_PER_PAGE
  #define BYTES_PER_PAGE            256           // Maximum number of bytes that can be written at once.
#endif

#ifndef   NUM_CLOCKS_CS_HIGH
  #define NUM_CLOCKS_CS_HIGH        4             // One less than the number of clocks which should be maintained between commands (depends on the connected NOR flassh)
#endif

#ifndef   CLOCK_RATE_MHZ
  #define CLOCK_RATE_MHZ            68            // Serial clock rate in MHz (PLL1 / 3)
#endif

#ifndef   API_ROM_ADDR
  #define API_ROM_ADDR              0x10400118    // Address where the pointer to SPIFI API is stored
#endif

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       Clock generation unit
*/
#define CGU_BASE_ADDR         0x40050000
#define IDIVA_CTRL            (*(volatile U32*)(CGU_BASE_ADDR + 0x048))
#define BASE_SPIFI_CLK        (*(volatile U32*)(CGU_BASE_ADDR + 0x070))

/*********************************************************************
*
*       System control unit
*/
#define SCU_BASE_ADDR         0x40086000
#define SFSP3_3               (*(volatile U32*)(SCU_BASE_ADDR + 0x18C))     // Pin configuration register for pin P3_3
#define SFSP3_4               (*(volatile U32*)(SCU_BASE_ADDR + 0x190))     // Pin configuration register for pin P3_4
#define SFSP3_5               (*(volatile U32*)(SCU_BASE_ADDR + 0x194))     // Pin configuration register for pin P3_5
#define SFSP3_6               (*(volatile U32*)(SCU_BASE_ADDR + 0x198))     // Pin configuration register for pin P3_6
#define SFSP3_7               (*(volatile U32*)(SCU_BASE_ADDR + 0x19C))     // Pin configuration register for pin P3_7
#define SFSP3_8               (*(volatile U32*)(SCU_BASE_ADDR + 0x1A0))     // Pin configuration register for pin P3_8

/*********************************************************************
*
*       Misc. defines
*/
#define CGU_IDIV            2
#define CGU_AUTOBLOCK       11
#define CGU_CLK_SEL         24
#define SFS_MODE            0
#define SFS_EPUN            4
#define SFS_EHS             5
#define SFS_EZI             6
#define SFS_ZIF             7
#define CLK_STAT_RUN        0
#define PERIPHERAL_RESET    2
#define NUM_RETRIES_INIT    3

/*********************************************************************
*
*       Option flags for the SPIFI ROM initialization function
*/
#define OPTION_CALLER_ERASE 0x008     // The caller is responsible for erasing the data before write
#define OPTION_FULL_CLK     0x040     // Data is sampled on rising edge
#define OPTION_RCV_CLK      0x080     // Data is sampled using the clock received from the pin
#define OPTION_MODE3        0x001     // Clock line is high in idle mode
#define OPTION_MINIMAL      0x002     // Use slowest read operation
#define OPTION_DUAL         0x100     // Use 2 bits for data transfer if supported
#define OPTION_CALLER_PROT  0x200     // The caller is responsible for restoring the data protection

/*********************************************************************
*
*       ASSERT_SECTOR_INDEX_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(pInst, SectorIndex)                         \
    if (SectorIndex >= pInst->NumSectors) {                                           \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Invalid sector index.")); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                            \
    }
#else
  #define ASSERT_SECTOR_INDEX_IS_IN_RANGE(pInst, SectorIndex)
#endif

/*********************************************************************
*
*       ASSERT_DEVICE_IS_SET
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_DEVICE_IS_SET(pInst)                                                 \
    if (pInst->pDevice == NULL) {                                                     \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Device not set."));       \
      FS_X_PANIC(FS_ERRCODE_UNKNOWN_DEVICE);                                          \
    }
#else
  #define ASSERT_DEVICE_IS_SET(pInst)
#endif

/*********************************************************************
*
*       ASSERT_UNIT_NO_IS_IN_RANGE
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_UNIT_NO_IS_IN_RANGE(Unit)                                            \
    if (Unit >= FS_NOR_NUM_UNITS) {                                                   \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Invalid unit number."));  \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                            \
    }
#else
  #define ASSERT_UNIT_NO_IS_IN_RANGE(Unit)
#endif

#if FS_NOR_NUM_PHY_SECTORS && BYTES_PER_PHY_SECTOR
  #define AUTO_DETECT_DEVICE  0
#else
  #define AUTO_DETECT_DEVICE  1
#endif

/*********************************************************************
*
*     Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*      Structures specified by the SPIFI API stored in ROM.
*/
typedef struct {
  U32    Base;
  U8     Flags;
  char   Log2;
  U16    Rept;
} SPIFI_PROT_ENTRY;

typedef union {
  U16    hw;
  U8     aByte[2];
} SPIFI_STAT;

typedef struct {
  void * pDest;
  U32    NumBytes;
  void * pScratch;
  int    Protect;
  U32    Options;
} SPIFI_PARA;

typedef struct {
  U32                Base;
  U32                RegBase;
  U32                DevSize;
  U32                MemSize;
  U8                 ManufacturerId;
  U8                 DeviceType;
  U8                 DeviceId;
  U8                 IsBusy;
  SPIFI_STAT         Stat;
  U16                Reserved;
  U16                SetProt;
  U16                WriteProt;
  U32                MemCmd;
  U32                ProgCmd;
  U16                Sectors;
  U16                ProtBytes;
  U32                Options;
  U32                ErrCheck;
  U8                 aEraseShift[4];
  U8                 aEraseOps[4];
  SPIFI_PROT_ENTRY * pProtEntries;
  char               aProt[68];
} SPIFI_OBJ;

typedef struct SPIFI_API {
  int (*pfInit)      (SPIFI_OBJ * pObj, U32 NumClocksCSHigh, U32 Options, U32 ClockRateMhz);
  int (*pfProgram)   (SPIFI_OBJ * pObj, void * pSource, SPIFI_PARA * pPara);
  int (*pfErase)     (SPIFI_OBJ * pObj,                 SPIFI_PARA * pPara);
} SPIFI_API;

typedef struct FLASH_DEVICE {
  U8  DeviceId;
  U32 SectorSize;
  U16 NumSectors;
  U16 BytesPerPage;
} FLASH_DEVICE;

typedef struct PHY_INST {
  U32                  BaseAddr;
  U32                  StartAddrConf;            // Configured start address
  U32                  StartAddrUsed;            // Start addr. actually used (aligned to start of a sector)
  U32                  NumBytes;
  const FLASH_DEVICE * pDevice;
  U16                  NumSectors;               // Total number of physical sectors
  U8                   IsInited;                 // Set to 1 if the physical layer is initialized
  U8                   Unit;
} PHY_INST;

/*********************************************************************
*
*      Static const
*
**********************************************************************
*/
#if AUTO_DETECT_DEVICE
static const FLASH_DEVICE _aDevice[] = {
  {0x11, 0x08000UL,   4, 256},   //   1MBit Device
  {0x12, 0x10000UL,   4, 256},   //   2MBit Device
  {0x13, 0x10000UL,   8, 256},   //   4MBit Device
  {0x14, 0x10000UL,  16, 256},   //   8MBit Device
  {0x15, 0x10000UL,  32, 256},   //  16MBit Device
  {0x16, 0x10000UL,  64, 256},   //  32MBit Device
  {0x17, 0x10000UL, 128, 256},   //  64MBit Device
  {0x18, 0x40000UL,  64, 256},   // 128MBit Device
  {0x19, 0x10000UL, 512, 256},   // 256MBit Device
  {0x00, 0x00000UL,   0,   0}
};
#else
static const FLASH_DEVICE _Device = {
  0, BYTES_PER_PHY_SECTOR, FS_NOR_NUM_PHY_SECTORS, BYTES_PER_PAGE
};
#endif

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static PHY_INST  * _apInst[FS_NOR_NUM_UNITS];
static SPIFI_OBJ   _Obj;
static SPIFI_API * _pAPI;

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _InitHW
*
*  Function description
*    Initializes SPIFI clock and port pins.
*/
static void _InitHW(void) {
  IDIVA_CTRL     = 9uL << CGU_CLK_SEL     // Use PLL1 as input
                 | 2uL << CGU_IDIV        // Divide PLL1 by 3
                 | 1uL << CGU_AUTOBLOCK
                 ;
  BASE_SPIFI_CLK = 12uL << CGU_CLK_SEL    // Use IDIVA
                 | 1uL  << CGU_AUTOBLOCK
                 ;
  //
  // Clock pin.
  //
  SFSP3_3 = 3uL << SFS_MODE
          | 1uL << SFS_EPUN
          | 1uL << SFS_EHS
          | 1uL << SFS_EZI
          | 1uL << SFS_ZIF
          ;
  //
  // Data pins.
  //
  SFSP3_4 = 3uL << SFS_MODE
          | 1uL << SFS_EPUN
          | 1uL << SFS_EZI
          | 1uL << SFS_ZIF
          ;
  SFSP3_5 = 3uL << SFS_MODE
          | 1uL << SFS_EPUN
          | 1uL << SFS_EZI
          | 1uL << SFS_ZIF
          ;
  SFSP3_6 = 3uL << SFS_MODE				// SPIFI_MISO
          | 1uL << SFS_EPUN
          | 1uL << SFS_EZI
          | 1uL << SFS_ZIF
          ;
  SFSP3_7 = 3uL << SFS_MODE
          | 1uL << SFS_EPUN
          | 1uL << SFS_EZI
          | 1uL << SFS_ZIF
          ;
  //
  // Chip select pin.
  //
  SFSP3_8 = 3uL << SFS_MODE
          | 1uL << SFS_EPUN
          ;
}

/*********************************************************************
*
*      _Init
*
*   Function description
*     Initializes the HW layer and auto-detects the SPI flash if not configured manually.
*/
static int _Init(PHY_INST * pInst) {
  U16 NumSectors;
  U32 SectorSize;
  U32 BaseAddr;
  U32 StartAddrUsed;
  I32 NumBytesToSkip;
  I32 NumBytesRem;
  U32 NumBytesSkipped;
  int r;
  int NumRetries;

  _InitHW();
  _pAPI = *((SPIFI_API **)API_ROM_ADDR);
  FS_MEMSET(&_Obj, 0, sizeof(_Obj));
  NumRetries = NUM_RETRIES_INIT;
  while (1) {
    r = _pAPI->pfInit(&_Obj, NUM_CLOCKS_CS_HIGH, OPTION_FULL_CLK | OPTION_RCV_CLK, CLOCK_RATE_MHZ);
    if (r == 0) {
      break;
    }
    if (NumRetries == 0) {
      break;
    }
    --NumRetries;
  }
  if (r) {
    //
    // Meaning of error codes:
    //
    // 0x2000A	No operative serial flash (JEDEC ID all zeroes or all ones)
    // 0x20009	Unknown manufacturer code
    // 0x20008	Unknown device type code
    // 0x20007	Unknown device ID code
    // 0x20006	Unknown extended device ID value (only for Spansion 25FL12x in the initial API)
    // 0x20005	Device status error
    // 0x20004	Operand error: S_MODE3+S_FULLCLK+S_RCVCLK in options
    //
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Could not initialize SPIFI (0x%08x).", r));
    return 1;               // Error, could not initialize the SPIFI interface.
  }
#if AUTO_DETECT_DEVICE
  {
    const FLASH_DEVICE * pDevice;
    //
    // Check Id code and write sector information to pInst.
    //
    FS_DEBUG_LOG((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Found serial NOR flash: ManufacturerId: 0x%2x, DeviceType: 0x%2x, DeviceId: 0x%2x\n", _Obj.ManufacturerId, _Obj.DeviceType, _Obj.DeviceId));
    pDevice = &_aDevice[0];
    do {
      if (pDevice->DeviceId == _Obj.DeviceId) {
        pInst->pDevice = pDevice;
        break;
      }
      pDevice++;
      if (pDevice->DeviceId == 0) {
        FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Could not identify device."));
        FS_X_PANIC(FS_ERRCODE_UNKNOWN_DEVICE);
      }
    } while (pDevice);
  }
#else
  pInst->pDevice = &_Device;
#endif
  //
  // OK, the device is identified. Determine which phyiscal sectors are used as storage.
  //
  NumBytesSkipped = 0;
  BaseAddr        = pInst->BaseAddr;
  NumBytesToSkip  = pInst->StartAddrConf - BaseAddr;
  NumBytesRem     = pInst->NumBytes;
  SectorSize      = pInst->pDevice->SectorSize;
  NumSectors      = pInst->pDevice->NumSectors;
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
    FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Flash size to small for this configuration. 0 bytes available."));
    return 1;               // Error, NOR flash too small.
  }
  pInst->StartAddrUsed = StartAddrUsed;
  pInst->NumSectors    = NumSectors;
  return 0;                 // OK, NOR flash is ready for data exchange.
}

/*********************************************************************
*
*      _InitIfRequired
*/
static int _InitIfRequired(PHY_INST * pInst) {
  int r;

  r = 0;
  if (pInst->IsInited == 0) {
    r = _Init(pInst);
    pInst->IsInited = 1;
  }
  return r;
}

/*********************************************************************
*
*      _AllocIfRequired
*
*   Function description
*     Allocates memory for the instance of a physical layer.
*
*/
static PHY_INST * _AllocIfRequired(U8 Unit) {
  PHY_INST * pInst;

  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst == NULL) {
     pInst = (PHY_INST *)FS_AllocZeroed(sizeof(PHY_INST));
     pInst->Unit  = Unit;
    _apInst[Unit] = pInst;
  }
  return pInst;
}

/*********************************************************************
*
*      Exported code
*
**********************************************************************
*/

/*********************************************************************
*
*      _WriteOff
*
*  Function description
*    Physical layer function.
*    This routine writes data into any section of the flash.
*    It does not check if this section has been previously erased;
*    this is in the responsibility of the user program.
*
*  Return value
*    ==0   O.K., data has been written
*    !=0   An error occurred
*/
static int _WriteOff(U8 Unit, U32 Off, const void * pSrc, U32 NumBytes) {
  U32 Addr;
  int r;
  PHY_INST * pInst;

  r = 1;                  // Set to indicate an error.
  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    if (NumBytes) {
      SPIFI_PARA Para;

      r = _InitIfRequired(pInst);
      if (r == 0) {
        Addr = pInst->StartAddrUsed + Off;
        //
        // Program the data by calling the ROM API function.
        //
        FS_MEMSET(&Para, 0, sizeof(Para));
        Para.pDest    = (void *)Addr;
        Para.NumBytes = NumBytes;
				Para.Options  = OPTION_CALLER_ERASE | OPTION_CALLER_PROT;
        r = _pAPI->pfProgram(&_Obj, (void *)pSrc, &Para);
        if (r) {
          //
          // Meaning of error codes:
          //
          // 0x20007	Programming and erasure cannot be done because the serial flash was not identified in the spifi_init operation.
          // 0x20005	Device status error
          // 0x20004	Operand error: the dest and/or length operands were out of range
          // 0x20003	Timeout waiting for program or erase to begin: protection could not be removed.
          // 0x20002	Internal error in API code.
          // 0x20001	Device error (not operating per specifications)
          // 0x2000B	S_CALLER_ERASE is included in options, and erasure is required.
          // other	  Other non-zero values can occur if options selects verification.
          //          They will be the address in the SPIFI memory area at which the first discrepancy was found.
          //
          FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI_LPC43: Could not program %lu bytes at 0x%08x (0x%08x).", NumBytes, Addr, r));
        }
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _ReadOff
*
*  Function description
*    Physical layer function.
*    Reads data from the given offset of the flash.
*/
static int _ReadOff(U8 Unit, void * pDest, U32 Off, U32 Len) {
  U32 Addr;
  int r;
  PHY_INST * pInst;

  r = 1;                  // Set to indicate an error.
  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    r = _InitIfRequired(pInst);
    if (r == 0) {
      Addr = pInst->StartAddrUsed + Off;
      FS_MEMCPY(pDest, (void *)Addr, Len);
      r = 0;
    }
  }
  return r;
}

/*********************************************************************
*
*      _EraseSector
*
*  Function description
*    Physical layer function.
*    Erases one physical sector.
*
*  Return value
*    ==0   O.K., sector has been erased
*    !=0   An error occurred
*/
static int _EraseSector(U8 Unit, unsigned int SectorIndex) {
  int r;
  U32 SectorSize;
  U32 SectorAddr;
  PHY_INST * pInst;

  r = 1;                    // Set to indicate an error.
  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    ASSERT_SECTOR_INDEX_IS_IN_RANGE(pInst, SectorIndex);
    ASSERT_DEVICE_IS_SET(pInst);
    r = _InitIfRequired(pInst);
    if (r == 0) {
      SPIFI_PARA Para;
      //
      // Get the sector size and compute the sector address.
      //
      SectorSize  = pInst->pDevice->SectorSize;
      SectorAddr  = SectorSize * SectorIndex;
      SectorAddr += pInst->StartAddrUsed;
      //
      // Erase the sector by calling the ROM API function.
      //
      FS_MEMSET(&Para, 0, sizeof(Para));
      Para.pDest    = (void *)SectorAddr;
      Para.NumBytes = SectorSize;
      r = _pAPI->pfErase(&_Obj, &Para);
      if (r) {
        FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER, "NOR_SPIFI: Could not erase %lu bytes at 0x%08x (0x%08x).", SectorSize, SectorAddr, r));
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _GetSectorInfo
*
*  Function description
*    Physical layer function. Returns the offset and length of the given physical sector.
*/
static void _GetSectorInfo(U8 Unit, unsigned int SectorIndex, U32 * pOff, U32 * pLen) {
  U32 SectorOff;
  U32 SectorSize;
  PHY_INST * pInst;

  SectorOff  = 0;
  SectorSize = 0;
  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    ASSERT_SECTOR_INDEX_IS_IN_RANGE(pInst, SectorIndex);
    ASSERT_DEVICE_IS_SET(pInst);
    _InitIfRequired(pInst);
    //
    // Compute result
    //
    SectorSize = pInst->pDevice->SectorSize;
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
*  Function description
*    Physical layer function. Returns the number total number of physical sectors in the SPI flash.
*/
static int _GetNumSectors(U8 Unit) {
  int NumSectors;
  PHY_INST * pInst;

  NumSectors = 0;               // Set to indicate an error.
  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    _InitIfRequired(pInst);
    NumSectors = pInst->NumSectors;
  }
  return NumSectors;
}

/*********************************************************************
*
*       _Configure
*
*  Function description
*    Physical layer function. Configures a single instance of the driver.
*/
static void _Configure(U8 Unit, U32 BaseAddr, U32 StartAddr, U32 NumBytes) {
  PHY_INST * pInst;

  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, StartAddr >= BaseAddr);
  pInst = _AllocIfRequired(Unit);
  if (pInst) {
    pInst->BaseAddr      = BaseAddr;
    pInst->StartAddrConf = StartAddr;
    pInst->NumBytes      = NumBytes;
    pInst->IsInited      = 0;         // The layer needs to be re-initialized.
  }
}

/*********************************************************************
*
*       _OnSelectPhy
*
*  Function description
*    Physical layer function.
*    Called right after selection of the physical layer.
*/
static void _OnSelectPhy(U8 Unit) {
  _AllocIfRequired(Unit);
}

/*********************************************************************
*
*       _DeInit
*
*  Function description
*    Physical layer function.
*    This function frees up memory resources allocated for the instance of a physical layer.
*
*  Parameters
*    Unit  Physical layer number
*/
static void _DeInit(U8 Unit) {
#if FS_SUPPORT_DEINIT
  PHY_INST * pInst;

  ASSERT_UNIT_NO_IS_IN_RANGE(Unit);
  pInst = _apInst[Unit];
  if (pInst) {
    if ((pInst->IsInited & (1 << 1))) {       // Has been manually configured?
      FS_Free((void *)((FLASH_DEVICE *)pInst->pDevice));
    }
    FS_Free((void *)pInst);
  }
  _apInst[Unit] = NULL;
#else
  FS_USE_PARA(Unit);
#endif
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_PHY_TYPE FS_NOR_PHY_SPIFI_NXP_LPC4357 = {
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
