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

File:    FS_NOR_HW_SPIFI_EFM32GG11_SiliconLabs_SLSTK3701A.c
Purpose: Sample NOR SPIFI hardware layer for the Silicon Labs SLSTK3701A evaluation board
Literature:
  [1] UG287: EFM32 Giant Gecko GG11 Starter Kit User's Guide
    (\\FILESERVER\Techinfo\Company\SiliconLabs\Evalboard\EFM32_GiantGecko\ug287-stk3701.pdf)
  [2] Schematics
    (\\FILESERVER\Techinfo\Company\SiliconLabs\Evalboard\EFM32_GiantGecko\EFMGG11-stk3701-schematic.pdf)
  [3] EFM32 Giant Gecko 11 Family Reference Manual
    (\\FILESERVER\Techinfo\Company\SiliconLabs\MCU\EFM32_GiantGecko11\efm32gg11-rm-05.pdf)
Additional information:
  This hardware layer uses functions of the emlib library provided by Silicon Labs.
*/

#include "FS.h"
#include "em_qspi.h"
#include "em_cmu.h"
#include "em_gpio.h"

/*********************************************************************
*
*      Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_HW_SPIFI_DATA_ADDR
  #define FS_NOR_HW_SPIFI_DATA_ADDR       0xC0000000          // This is the start address of the memory region used by the file system
                                                              // to read the data from the serial NOR flash device. The hardware layer
                                                              // performs a dummy read from this address when switching to memory mode
                                                              // in order to clean the caches. It should be set to the value passed
                                                              // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

/*********************************************************************
*
*      Defines, fixed
*
**********************************************************************
*/
#define DATA_ALIGNMENT      4u

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _GetTramsferType
*/
static QSPI_TransferType_TypeDef _GetTramsferType(unsigned BusWidth) {
  QSPI_TransferType_TypeDef r;

  r = qspiTransferSingle;
  if (BusWidth == 1) {
    r = qspiTransferSingle;
  } else {
    if (BusWidth == 2) {
      r = qspiTransferDual;
    } else {
      if (BusWidth == 4) {
        r = qspiTransferQuad;
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _GetNumCycles
*/
static U32 _GetNumCycles(unsigned BusWidth, U32 NumBytes) {
  U32 NumCycles;

  NumCycles = 0;
  if (NumBytes) {
    NumCycles = NumBytes << 3;        // Assume 8-bits per byte.
    switch (BusWidth) {
    case 2:
      NumCycles >>= 1;
      break;
    case 4:
      NumCycles >>= 2;
      break;
    default:
      break;
    }
  }
  return NumCycles;
}

/*********************************************************************
*
*       _ReadData
*
*  Function description
*    Reads data from the NOR flash device.
*
*  Notes
*    (1) This function can read a maximum of 8 data bytes.
*        This is a limitation of the STIG mode of the QSPI controller.
*/
static void _ReadData(U8 Cmd, unsigned NumCyclesDummy, U32 Addr, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  QSPI_StigCmd_TypeDef    ConfigCmd;
  QSPI_ReadConfig_TypeDef ConfigRead;

  //
  // Configure the read parameters.
  //
  FS_MEMSET(&ConfigRead, 0, sizeof(ConfigRead));
  ConfigRead.dataTransfer = _GetTramsferType(FS_BUSWIDTH_GET_DATA(BusWidth));
  ConfigRead.addrTransfer = _GetTramsferType(FS_BUSWIDTH_GET_ADDR(BusWidth));
  ConfigRead.instTransfer = _GetTramsferType(FS_BUSWIDTH_GET_CMD(BusWidth));
  ConfigRead.dummyCycles  = NumCyclesDummy;
  ConfigRead.opCode       = Cmd;
  QSPI_ReadConfig(QSPI0, &ConfigRead);
  //
  // Configure the command parameters and execute the read command.
  //
  FS_MEMSET(&ConfigCmd, 0, sizeof(ConfigCmd));
  ConfigCmd.cmdOpcode    = Cmd;
  ConfigCmd.addrSize     = NumBytesAddr;
  ConfigCmd.address      = Addr;
  ConfigCmd.readDataSize = NumBytesData;
  ConfigCmd.readBuffer   = pData;
  ConfigCmd.dummyCycles  = NumCyclesDummy;
  QSPI_ExecStigCmd(QSPI0, &ConfigCmd);
}

/*********************************************************************
*
*       _ReadDataStandardMode
*
*  Function description
*    Reads data from NOR flash using the standard read command via a single data line.
*/
static void _ReadDataStandardMode(U32 Addr, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData) {
  _ReadData(0x03, 8, Addr, NumBytesAddr, pData, NumBytesData, FS_BUSWIDTH_MAKE(1, 1, 1));
}

/*********************************************************************
*
*       _WriteData
*
*  Function description
*    Writes data to NOR flash device.
*
*  Additional information
*    This function is able to write data that is not aligned to a
*    32-bit boundary.
*/
static void _WriteData(U8 Cmd, unsigned NumCyclesDummy, U32 Addr, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  QSPI_WriteConfig_TypeDef   ConfigWrite;
  U32                        Status;
  U32                        NumItems;
  U32                      * pData32;
  volatile U32             * pMem32;
  U32                        AddrMem;
  U32                        Data32;
  U32                        NumBytesToWrite;
  U32                        NumBytesRem;
  U32                        AddrWrite;
  U32                        NumBytesMiddle;
  U32                        NumBytesTail;
  U32                        NumBytesHead;
  U32                        NumBytesToRead;
  U8                         abDataHead[DATA_ALIGNMENT - 1];
  U8                         abDataTail[DATA_ALIGNMENT - 1];
  U32                        DataHead;
  U32                        DataTail;
  U32                        AddrTail;
  const U8                 * pDataMiddle;

  //
  // Configure the command parameters.
  //
  FS_MEMSET(&ConfigWrite, 0, sizeof(ConfigWrite));
  ConfigWrite.dataTransfer = _GetTramsferType(FS_BUSWIDTH_GET_CMD(BusWidth));
  ConfigWrite.addrTransfer = _GetTramsferType(FS_BUSWIDTH_GET_ADDR(BusWidth));
  ConfigWrite.dataTransfer = _GetTramsferType(FS_BUSWIDTH_GET_DATA(BusWidth));
  ConfigWrite.dummyCycles  = (U8)NumCyclesDummy;
  ConfigWrite.opCode       = Cmd;
  ConfigWrite.autoWEL      = false;                 // The NOR physical layer queries the completion of the write operation.
  QSPI_WriteConfig(QSPI0, &ConfigWrite);
  //
  // Detect an unaligned write operation and handle it accordingly.
  //
  NumBytesToWrite = NumBytesData;
  NumBytesRem     = NumBytesData;
  AddrWrite       = Addr;
  NumBytesHead    = 0;
  NumBytesMiddle  = 0;
  NumBytesTail    = 0;
  DataHead        = 0;
  DataTail        = 0;
  pDataMiddle     = pData;
  //
  // Process unaligned heading bytes.
  //
  NumBytesToRead  = Addr & (DATA_ALIGNMENT - 1);
  if (NumBytesToRead != 0u) {
    NumBytesHead     = DATA_ALIGNMENT - NumBytesToRead;
    AddrWrite       -= NumBytesToRead;
    NumBytesToWrite += NumBytesToRead;
    NumBytesRem     -= NumBytesHead;
    FS_MEMSET(abDataHead, 0, sizeof(abDataHead));
    _ReadDataStandardMode(AddrWrite, NumBytesAddr, abDataHead, NumBytesToRead);
    if (NumBytesToRead == 1) {
      DataHead  = (U32)abDataHead[0];
      DataHead |= (U32)*pData++ << 8;
      DataHead |= (U32)*pData++ << 16;
      DataHead |= (U32)*pData++ << 24;
    } else {
      if (NumBytesToRead == 2) {
        DataHead  = (U32)abDataHead[0];
        DataHead |= (U32)abDataHead[1] << 8;
        DataHead |= (U32)*pData++ << 16;
        DataHead |= (U32)*pData++ << 24;
      } else {
        if (NumBytesToRead == 3) {
          DataHead  = (U32)abDataHead[0];
          DataHead |= (U32)abDataHead[1] << 8;
          DataHead |= (U32)abDataHead[2] << 16;
          DataHead |= (U32)*pData++ << 24;
        }
      }
    }
  }
  //
  // Calculate the number of aligned bytes.
  //
  if (NumBytesRem != 0u) {
    NumBytesMiddle  = NumBytesRem & ~(DATA_ALIGNMENT - 1);
    pDataMiddle     = pData;
    NumBytesRem    -= NumBytesMiddle;
    pData          += NumBytesMiddle;
  }
  //
  // Process unaligned trailing bytes.
  //
  if (NumBytesRem != 0u) {
    NumBytesTail   = NumBytesRem;
    NumBytesToRead = DATA_ALIGNMENT - NumBytesTail;
    AddrTail       = AddrWrite + NumBytesToWrite;
    FS_MEMSET(abDataTail, 0, sizeof(abDataTail));
    _ReadDataStandardMode(AddrTail, NumBytesAddr, abDataTail, NumBytesToRead);
    if (NumBytesToRead == 1) {
      DataTail  = (U32)*pData++;
      DataTail |= (U32)*pData++ << 8;
      DataTail |= (U32)*pData++ << 16;
      DataTail |= (U32)abDataTail[0] << 24;
    } else {
      if (NumBytesToRead == 2) {
        DataTail  = (U32)*pData++;
        DataTail |= (U32)*pData++ << 8;
        DataTail |= (U32)abDataTail[0] << 16;
        DataTail |= (U32)abDataTail[1] << 24;
      } else {
        if (NumBytesToRead == 3) {
          DataTail  = (U32)*pData++;
          DataTail |= (U32)abDataTail[0] << 8;
          DataTail |= (U32)abDataTail[1] << 16;
          DataTail |= (U32)abDataTail[2] << 24;
        }
      }
    }
    NumBytesToWrite += DATA_ALIGNMENT - NumBytesTail;
  }
  AddrMem = FS_NOR_HW_SPIFI_DATA_ADDR + AddrWrite;
  //
  // Configure the data transfer parameters.
  //
  QSPI0->DEVSIZECONFIG             &= ~(_QSPI_DEVSIZECONFIG_NUMADDRBYTES_MASK << _QSPI_DEVSIZECONFIG_NUMADDRBYTES_SHIFT);
  QSPI0->DEVSIZECONFIG             |=  ((NumBytesAddr - 1)                    << _QSPI_DEVSIZECONFIG_NUMADDRBYTES_SHIFT);
  QSPI0->WRITECOMPLETIONCTRL       |= QSPI_WRITECOMPLETIONCTRL_DISABLEPOLLING;  // Disable the automatic polling mode because this is done by the physical layer.
  QSPI0->INDIRECTWRITEXFERSTART     = AddrWrite;
  QSPI0->INDIRECTWRITEXFERNUMBYTES  = NumBytesToWrite;
  QSPI0->INDAHBADDRTRIGGER          = AddrMem;
  QSPI0->INDIRECTTRIGGERADDRRANGE   = 0xF;        // Set to maximum value because the documentation is not very clear about how this value is used.
  //
  // Start the data transfer.
  //
  QSPI0->INDIRECTWRITEXFERCTRL = QSPI_INDIRECTWRITEXFERCTRL_START;
  for (;;) {
    Status = QSPI0->INDIRECTWRITEXFERCTRL;
    if ((Status & QSPI_INDIRECTWRITEXFERCTRL_START) == 0u) {
      break;
    }
  }
  //
  // Write the data. By default, the write FIFO is 512 bytes large
  // therefore we can write directly without checking the FIFO fill
  // status because almost all of the popular NOR flash devices have
  // a page size of 256 bytes.
  //
  if (   (((U32)pData & (DATA_ALIGNMENT - 1u)) == 0)
      && (NumBytesHead == 0u)
      && (NumBytesTail == 0u)) {
    //
    // Write aligned data.
    //
    pData32  = (U32 *)pDataMiddle;
    pMem32   = (U32 *)AddrMem;
    NumItems = NumBytesMiddle >> 2;
    do {
      *pMem32++ = *pData32++;
    } while (--NumItems != 0);
  } else {
    //
    // Write unaligned data.
    //
    pMem32 = (U32 *)AddrMem;
    if (NumBytesHead != 0u) {
      *pMem32++ = DataHead;
    }
    if (NumBytesMiddle != 0) {
      NumItems = NumBytesMiddle >> 2;
      do {
        Data32  = (U32)*pDataMiddle++;
        Data32 |= (U32)*pDataMiddle++ << 8;
        Data32 |= (U32)*pDataMiddle++ << 16;
        Data32 |= (U32)*pDataMiddle++ << 24;
        *pMem32++ = Data32;
      } while (--NumItems != 0);
    }
    if (NumBytesTail != 0u) {
      *pMem32++ = DataTail;
    }
  }
  //
  // Wait for the data transfer to finish.
  //
  for (;;) {
    Status = QSPI0->INDIRECTWRITEXFERCTRL;
    if ((Status & QSPI_INDIRECTWRITEXFERCTRL_INDOPSDONESTATUS) != 0u) {
      QSPI0->INDIRECTWRITEXFERCTRL = QSPI_INDIRECTWRITEXFERCTRL_INDOPSDONESTATUS;
      break;
    }
  }
}

/*********************************************************************
*
*      Public code (via callback)
*
**********************************************************************
*/

/*********************************************************************
*
*       _HW_Init
*
*  Function description
*    HW layer function. It is called before any other function of the physical layer.
*    It should configure the HW so that the other functions can access the NOR flash.
*
*  Return value
*    Frequency of the SPI clock in Hz.
*/
static int _HW_Init(U8 Unit) {
  QSPI_Init_TypeDef Config;

  FS_USE_PARA(Unit);
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_QSPI0, true);
  GPIO_PinModeSet(gpioPortG, 13, gpioModePushPull, 1);  // PWR_EN
  GPIO_PinModeSet(gpioPortG, 9, gpioModePushPull, 0);   // CS
  GPIO_PinModeSet(gpioPortG, 0, gpioModePushPull, 0);   // CLK
  GPIO_PinModeSet(gpioPortG, 1, gpioModePushPull, 0);   // DQ0
  GPIO_PinModeSet(gpioPortG, 2, gpioModePushPull, 0);   // DQ1
  GPIO_PinModeSet(gpioPortG, 3, gpioModePushPull, 0);   // DQ2
  GPIO_PinModeSet(gpioPortG, 4, gpioModePushPull, 0);   // DQ3
  QSPI0->ROUTELOC0 = QSPI_ROUTELOC0_QSPILOC_LOC2;
  QSPI0->ROUTEPEN  = 0u
                   | QSPI_ROUTEPEN_SCLKPEN
                   | QSPI_ROUTEPEN_CS0PEN
                   | QSPI_ROUTEPEN_DQ0PEN
                   | QSPI_ROUTEPEN_DQ1PEN
                   | QSPI_ROUTEPEN_DQ2PEN
                   | QSPI_ROUTEPEN_DQ3PEN
                   ;
  QSPI0->DEVDELAY = 0u
                  | (1uL << _QSPI_DEVDELAY_DINIT_SHIFT)
                  | (1uL << _QSPI_DEVDELAY_DAFTER_SHIFT)
                  | (1uL << _QSPI_DEVDELAY_DBTWN_SHIFT)
                  | (1uL << _QSPI_DEVDELAY_DNSS_SHIFT)
                  ;
  Config.divisor = 4;
  Config.enable  = true;
  QSPI_Init(QSPI0, &Config);
  return CMU_ClockFreqGet(cmuClock_QSPI0);
}

/*********************************************************************
*
*       _HW_SetCmdMode
*
*  Function description
*    HW layer function. It enables the direct access to NOR flash via SPI.
*    This function disables the memory-mapped mode.
*/
static void _HW_SetCmdMode(U8 Unit) {
  FS_USE_PARA(Unit);
  //
  // The memory mapped mode does not have to be disabled
  // when the NOR flash device is accessed in indirect mode.
  //
}

/*********************************************************************
*
*       _HW_SetMemMode
*
*  Function description
*    HW layer function. It enables the memory-mapped mode. In this mode
*    the data can be accessed by doing read operations from memory.
*    The HW is responsible to transfer the data via SPI.
*    This function disables the direct access to NOR flash via SPI.
*/
static void _HW_SetMemMode(U8 Unit, U8 ReadCmd, unsigned NumBytesAddr, unsigned NumBytesDummy, U16 BusWidth) {
  QSPI_ReadConfig_TypeDef Config;
  U32                     NumCyclesDummy;

  FS_USE_PARA(Unit);
  NumCyclesDummy = _GetNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesDummy);  // The dummy bytes are sent using the data mode.
  QSPI0->DEVSIZECONFIG &= ~(_QSPI_DEVSIZECONFIG_NUMADDRBYTES_MASK << _QSPI_DEVSIZECONFIG_NUMADDRBYTES_SHIFT);
  QSPI0->DEVSIZECONFIG |=  ((NumBytesAddr - 1)                    << _QSPI_DEVSIZECONFIG_NUMADDRBYTES_SHIFT);
  FS_MEMSET(&Config, 0, sizeof(Config));
  Config.dataTransfer = _GetTramsferType(FS_BUSWIDTH_GET_DATA(BusWidth));
  Config.addrTransfer = _GetTramsferType(FS_BUSWIDTH_GET_ADDR(BusWidth));
  Config.instTransfer = _GetTramsferType(FS_BUSWIDTH_GET_CMD(BusWidth));
  Config.dummyCycles  = (U8)NumCyclesDummy;
  Config.opCode       = ReadCmd;
  QSPI_ReadConfig(QSPI0, &Config);
}

/*********************************************************************
*
*       _HW_ExecCmd
*
*  Function description
*    HW layer function. It requests the NOR flash to execute a simple command.
*    The HW has to be in SPI mode.
*/
static void _HW_ExecCmd(U8 Unit, U8 Cmd, U8 BusWidth) {
  QSPI_StigCmd_TypeDef ConfigCmd;

  FS_USE_PARA(Unit);
  FS_USE_PARA(BusWidth);
  FS_MEMSET(&ConfigCmd, 0, sizeof(ConfigCmd));
  ConfigCmd.cmdOpcode = Cmd;
  QSPI_ExecStigCmd(QSPI0, &ConfigCmd);
}

/*********************************************************************
*
*       _HW_ReadData
*
*  Function description
*    HW layer function. It transfers data from NOR flash to MCU.
*    The HW has to be in SPI mode.
*/
static void _HW_ReadData(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  U32 NumCyclesDummy;
  U32 NumBytesDummy;
  U32 Addr;
  U32 NumBytes;

  FS_USE_PARA(Unit);
  //
  // Calculate the number of dummy clock cycles.
  //
  NumBytesDummy  = NumBytesPara - NumBytesAddr;
  NumCyclesDummy = _GetNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesDummy);  // The dummy bytes are sent using the data mode.
  //
  // Encode the address.
  //
  Addr = 0;
  NumBytes = NumBytesAddr;
  if (NumBytes != 0u) {
    do {
      Addr <<= 8;
      Addr  |= (U32)*pPara++;
    } while (--NumBytes != 0u);
  }
  _ReadData(Cmd, NumCyclesDummy, Addr, NumBytesAddr, pData, NumBytesData, BusWidth);
}

/*********************************************************************
*
*       _HW_WriteData
*
*  Function description
*    HW layer function. It transfers data from MCU to NOR flash.
*    The HW has to be in SPI mode.
*/
static void _HW_WriteData(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  QSPI_StigCmd_TypeDef ConfigCmd;
  U32                  NumCyclesDummy;
  U32                  NumBytesDummy;
  U32                  Addr;
  U32                  NumBytes;

  FS_USE_PARA(Unit);
  //
  // Calculate the number of dummy clock cycles.
  //
  NumBytesDummy  = NumBytesPara - NumBytesAddr;
  NumCyclesDummy = _GetNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesDummy);  // The dummy bytes are sent using the data mode.
  //
  // Encode the address.
  //
  Addr = 0;
  NumBytes = NumBytesAddr;
  if (NumBytes != 0u) {
    do {
      Addr <<= 8;
      Addr  |= (U32)*pPara++;
    } while (--NumBytes != 0u);
  }
  //
  // The QSPI controller is able to exchange at most 8 bytes in STIG mode.
  // Therefore we have to use the indirect mode when the NOR physical layer
  // wants to write more than 8 bytes at once.
  //
  if (NumBytesData <= 8) {
    FS_MEMSET(&ConfigCmd, 0, sizeof(ConfigCmd));
    ConfigCmd.cmdOpcode     = Cmd;
    ConfigCmd.dummyCycles   = NumCyclesDummy;
    ConfigCmd.addrSize      = NumBytesAddr;
    ConfigCmd.address       = Addr;
    ConfigCmd.writeDataSize = NumBytesData;
    ConfigCmd.writeBuffer   = (void *)pData;
    QSPI_ExecStigCmd(QSPI0, &ConfigCmd);
  } else {
    _WriteData(Cmd, NumCyclesDummy, Addr, NumBytesAddr, pData, NumBytesData, BusWidth);
  }
}

/*********************************************************************
*
*       _HW_Delay
*
*  Function description
*    HW layer function. Blocks the execution for the specified number
*    of milliseconds.
*
*  Return value
*    ==0    Delay executed.
*    < 0    Feature not supported.
*/
static int _HW_Delay(U8 Unit, U32 ms) {
  int r;

  FS_USE_PARA(Unit);
  FS_USE_PARA(ms);
  r = -1;                 // Set to indicate that the feature is not supported.
  return r;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_EFM32GG11_SiliconLabs_SLSTK3701A = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
  NULL,
  _HW_Delay,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
