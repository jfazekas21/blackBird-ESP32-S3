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

File    : FS_NOR_HW_SPIFI_R7FA6M4_Renesas_RA6M4_EK.c
Purpose : Hardware layer for Renesas RA6M4 and quad SPI NOR flash.
Literature  :
  [1] RA6M4 Group User's Manual: Hardware
    (\\fileserver\Techinfo\Company\Renesas\MCU\RA_Series\RA6M4\RA6M4_UserManualHardware.pdf)
  [2] Datasheet RA6M4 Group Renesas Microcontrollers
    (\\fileserver\Techinfo\Company\Renesas\MCU\RA_Series\RA6M4\RA6M4_DataSheet.pdf)
  [3] Evaluation Kit for RA6M4 Microcontroller Group EK-RA6M4 v1 User's Manual
    (\\fileserver\Techinfo\Company\Renesas\MCU\RA_Series\RA6M4\REN_r20ut4836eg0100-ek-ra6m4-v1-um_MAH_20200824.pdf)
*/

/*********************************************************************
*
*      #include section
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
#ifndef   FS_NOR_HW_PER_CLK_HZ
  #define FS_NOR_HW_PER_CLK_HZ              100000000         // Clock of Quad-SPI peripheral
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ              50000000          // Frequency of the clock supplied to NOR flash device
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       QSPI controller
*/
#define QSPI_BASE_ADDR                0x64000000uL
#define QSPI_SFMSMD                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0000))    // Transfer Mode Control Register
#define QSPI_SFMSSC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0004))    // Chip Selection Control Register
#define QSPI_SFMSKC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0008))    // Clock Control Register
#define QSPI_SFMSST                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x000C))    // Status Register
#define QSPI_SFMCOM                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0010))    // Communication Port Register
#define QSPI_SFMCMD                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0014))    // Communication Mode Control Register
#define QSPI_SFMCST                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0018))    // Communication Status Register
#define QSPI_SFMSIC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0020))    // Instruction Code Register
#define QSPI_SFMSAC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0024))    // Address Mode Control Register
#define QSPI_SFMSDC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0028))    // Dummy Cycle Control Register
#define QSPI_SFMSPC                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0030))    // SPI Protocol Control Register
#define QSPI_SFMPMD                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0034))    // Port Control Register
#define QSPI_SFMCNT1                  (*(volatile U32 *)(QSPI_BASE_ADDR + 0x0804))    // External QSPI Address Register

/*********************************************************************
*
*       Port Pin Function Select Register
*/
#define PFS_BASE_ADDR                 0x40080800uL
#define PFS_P305PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xD4))
#define PFS_P306PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xD8))
#define PFS_P307PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xDC))
#define PFS_P308PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xE0))
#define PFS_P309PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xE4))
#define PFS_P310PFS                   (*(volatile U32 *)(PFS_BASE_ADDR + 0xE8))

/*********************************************************************
*
*       Misc. registers
*/
#define MSTP_MSTPCRB                  (*(volatile U32 *)0x40084004)     // Module Stop Control Register B
#define PMISC_PWPR                    (*(volatile U8  *)0x40080D03)
#define SYS_PRCR                      (*(volatile U16 *)0x4001E3FE)

/*********************************************************************
*
*       Pin function register
*/
#define PFS_DSCR_BIT                  10
#define PFS_DSCR_HIGH                 0x03uL
#define PFS_PMR_BIT                   16
#define PFS_PSEL_BIT                  24
#define PFS_PSEL_MASK                 0x1FuL
#define PFS_PSEL_QSPI                 0x11uL

/*********************************************************************
*
*       Transfer mode control
*/
#define SFMSMD_SFMRM_READ             0uL
#define SFMSMD_SFMRM_FAST_READ        1uL
#define SFMSMD_SFMRM_FAST_READ_DO     2uL
#define SFMSMD_SFMRM_FAST_READ_DIO    3uL
#define SFMSMD_SFMRM_FAST_READ_QO     4uL
#define SFMSMD_SFMRM_FAST_READ_QIO    5uL
#define SFMSMD_SFMPFE_BIT             6
#define SFMSMD_SFMMD3_BIT             8
#define SFMSMD_SFMCCE_BIT             15

/*********************************************************************
*
*       Clock control
*/
#define SFMSKC_SFMDV_BIT              0
#define SFMSKC_SFMDV_MAX              0x1FuL
#define SFMSKC_SFMDTY_BIT             5

/*********************************************************************
*
*       Address mode control
*/
#define SFMSAC_SFMAS_1BYTE            0uL
#define SFMSAC_SFMAS_2BYTES           1uL
#define SFMSAC_SFMAS_3BYTES           2uL
#define SFMSAC_SFMAS_4BYTES           3uL

/*********************************************************************
*
*       Dummy cycle control
*/
#define SFMSDC_SFMDN_BIT              0
#define SFMSDC_SFMXD_BIT              8

/*********************************************************************
*
*       Register access locking
*/
#define PRCR_PRCR0_BIT        0
#define PRCR_PRCR1_BIT        1
#define PRCR_PRCR2_BIT        2
#define PRCR_PRCR3_BIT        3
#define PRCR_LOCK_MASK        (  (1u << PRCR_PRCR0_BIT) \
                               | (1u << PRCR_PRCR1_BIT) \
                               | (1u << PRCR_PRCR2_BIT) \
                               | (1u << PRCR_PRCR3_BIT))
#define PRCR_KEY_VALUE        0xA5u
#define PRCR_KEY_BIT          8

/*********************************************************************
*
*       Misc. defines
*/
#define MSTPCRB_QSPI_BIT              6
#define NUM_LOOPS_TIMEOUT             10000000
#define PWPR_B0WI_BIT                 7
#define PWPR_PFSWE_BIT                6
#define SFMPMD_SFMWPL_BIT             2
#define SFMSPC_SFMSDE_BIT             4
#define SFMCMD_DCOM_BIT               0
#define SFMSMD_SFMSE_BIT              4
#define SFMSMD_SFMSE_QSSL_EXT         3uL
#define MIN_NUM_CYCLES_DUMMY          3
#define MAX_NUM_CYCLES_DUMMY          17

/*********************************************************************
*
*       ASSERT_BUS_WIDTH1
*/
#if FS_DEBUG_LEVEL >= FS_DEBUG_LEVEL_CHECK_ALL
  #define ASSERT_BUS_WIDTH1(BusWidth)                                                         \
    if ((((U16)(BusWidth) & 0xFu) > 0x1u) || (((U16)(BusWidth) & 0xF0u) > 0x10u)) {           \
      FS_DEBUG_ERROROUT((FS_MTYPE_DRIVER,  "NOR_HW_RA6M4: Invalid bus width %x.", BusWidth)); \
      FS_X_PANIC(FS_ERRCODE_INVALID_PARA);                                                    \
    }
#else
  #define ASSERT_BUS_WIDTH1(BusWidth)
#endif

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _EnableQSPI
*
*  Function description
*    Enables the clock to SDHI modules.
*/
static int _EnableQSPI(void) {
  U32 Mask;
  U32 TimeOut;
  int r;
  U32 MaskLock;

  MaskLock = SYS_PRCR & PRCR_LOCK_MASK;
  if (MaskLock != PRCR_LOCK_MASK) {
    SYS_PRCR = 0                // Unlock the access to all registers.
         | (PRCR_KEY_VALUE << PRCR_KEY_BIT)
         | (PRCR_LOCK_MASK << PRCR_PRCR0_BIT)
         ;
  }
  r = 0;
  Mask = 1uL << MSTPCRB_QSPI_BIT;
  //
  // Stop the peripheral first.
  //
  if ((MSTP_MSTPCRB & Mask) == 0) {
    MSTP_MSTPCRB |= Mask;
    TimeOut = NUM_LOOPS_TIMEOUT;
    while (1) {
      if (MSTP_MSTPCRB & Mask) {
        break;
      }
      if (--TimeOut == 0) {
        r = 1;              // Error, cold not stop module.
        break;
      }
    }
  }
  //
  // Start the peripheral.
  //
  MSTP_MSTPCRB &= ~Mask;
  TimeOut = NUM_LOOPS_TIMEOUT;
  while (1) {
    if ((MSTP_MSTPCRB & Mask) == 0) {
      break;
    }
    if (--TimeOut == 0) {
      r = 1;                // Error, cold not start module.
      break;
    }
  }
  if (MaskLock != PRCR_LOCK_MASK) {
    SYS_PRCR = 0            // Lock the access to the registers marked as locked.
         | (PRCR_KEY_VALUE << PRCR_KEY_BIT)
         | (MaskLock       << PRCR_PRCR0_BIT)
         ;
  }
  return r;
}

/*********************************************************************
*
*       _ConfigurePortPins
*
*  Function description
*    Configures the port pins for the QSPI operation.
*/
static void _ConfigurePortPins(void) {
  int IsWP;

  //
  // Disable write protection if required.
  //
  IsWP = 0;
  if (PMISC_PWPR & (1 << PWPR_B0WI_BIT)) {
    IsWP = 1;
  }
  if (IsWP) {
    PMISC_PWPR &= (U8)~(1 << PWPR_B0WI_BIT);
    PMISC_PWPR |= 1 << PWPR_PFSWE_BIT;
  }
  //
  // Assign pins to QSPI peripheral.
  //
  PFS_P305PFS = 0                 // QSPI CLK
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  PFS_P306PFS = 0                 // QSPI CS#
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  PFS_P307PFS = 0                 // QSPI DQ0
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  PFS_P308PFS = 0                 // QSPI DQ1
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  PFS_P309PFS = 0                 // QSPI DQ2
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  PFS_P310PFS = 0                 // QSPI DQ3
              | (1uL           << PFS_PMR_BIT)
              | (PFS_DSCR_HIGH << PFS_DSCR_BIT)
              | (PFS_PSEL_QSPI << PFS_PSEL_BIT)
              ;
  //
  // Enable write protection if required.
  //
  if (IsWP) {
    PMISC_PWPR |= 1 << PWPR_B0WI_BIT;
  }
}

/*********************************************************************
*
*       _SetClockSpeed
*
*  Function description
*    Configures the speed of the clock supplied NOR flash device.
*
*  Return value
*    Frequency of the SPI clock in Hz.
*/
static U32 _SetClockSpeed(unsigned MaxFreq_Hz) {
  U32 Div;
  U32 Freq_Hz;
  U32 Ratio;

  Div   = 0;
  Ratio = 2;
  while (1) {
    Freq_Hz = FS_NOR_HW_PER_CLK_HZ / Ratio;
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    if (Ratio < 18) {
      ++Ratio;
    } else {
      Ratio += 2;
    }
    ++Div;
    if (Div >= SFMSKC_SFMDV_MAX) {
      Div = SFMSKC_SFMDV_MAX;
      Freq_Hz = FS_NOR_HW_PER_CLK_HZ / Div;
      break;
    }
  }
  QSPI_SFMSKC = 0
              | (Div << SFMSKC_SFMDV_BIT)
              | (1uL << SFMSKC_SFMDTY_BIT)      // Use equal clock duty ratio for odd dividers.
              ;
  return Freq_Hz;
}

/*********************************************************************
*
*       _ConfigureQSPI
*/
static int _ConfigureQSPI(void) {
  int Freq_Hz;

  QSPI_SFMSMD  = 0
               | (1uL                   << SFMSMD_SFMPFE_BIT)   // Read data in advance.
               | (1uL                   << SFMSMD_SFMMD3_BIT)   // Clock signal is HIGH in idle mode.
               | (1uL                   << SFMSMD_SFMCCE_BIT)   // Do not use default instruction codes.
               | (SFMSMD_SFMSE_QSSL_EXT << SFMSMD_SFMSE_BIT)    // Keep CS active after read access.
               ;
  QSPI_SFMSSC  = 0;
  QSPI_SFMSDC  = 0;
  QSPI_SFMCMD  = 0;
  QSPI_SFMCST  = 0;
  QSPI_SFMSAC  = 0;
  QSPI_SFMSPC  = 1uL << SFMSPC_SFMSDE_BIT;
  QSPI_SFMPMD  = 1uL << SFMPMD_SFMWPL_BIT;        // Make sure that the NOR flash device is writable.
  QSPI_SFMCNT1 = 0;
  Freq_Hz = (int)_SetClockSpeed(FS_NOR_HW_NOR_CLK_HZ);
  return Freq_Hz;
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
  int Freq_Hz;

  FS_USE_PARA(Unit);
  //
  // Start the QSPI peripheral.
  //
  _EnableQSPI();
  //
  // Configure the I/O ports.
  //
  _ConfigurePortPins();
  //
  // Configure the QSPI peripheral.
  //
  Freq_Hz = _ConfigureQSPI();
  return Freq_Hz;
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
  QSPI_SFMCMD |= 1uL << SFMCMD_DCOM_BIT;        // Enter direct communication mode.
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
  U32 AddrMode;
  U32 ReadMode;
  U32 NumCyclesDummy;

  FS_USE_PARA(Unit);
  switch (NumBytesAddr) {
  case 1:
    AddrMode = SFMSAC_SFMAS_1BYTE;
    break;
  case 2:
    AddrMode = SFMSAC_SFMAS_2BYTES;
    break;
  case 3:
    // through
  default:
    AddrMode = SFMSAC_SFMAS_3BYTES;
    break;
  case 4:
    AddrMode = SFMSAC_SFMAS_4BYTES;
    break;
  }
  if (BusWidth == 0x112u) {
    ReadMode = SFMSMD_SFMRM_FAST_READ_DO;      // Data field is dual other fields are serial.
  } else if (BusWidth == 0x114u) {
    ReadMode = SFMSMD_SFMRM_FAST_READ_QO;      // Data field is quad other fields are serial.
  } else if (BusWidth == 0x122u) {
    ReadMode = SFMSMD_SFMRM_FAST_READ_DIO;     // Opcode field is serial. Other fields are dual.
  } else if (BusWidth == 0x144u) {
    ReadMode = SFMSMD_SFMRM_FAST_READ_QIO;     // Opcode field is serial. Other fields are quad.
  } else {
    ReadMode = SFMSMD_SFMRM_FAST_READ;         // All fields of the command are serial.
  }
  NumCyclesDummy = NumBytesDummy << 3;
  if (BusWidth == 0x122u) {
    NumCyclesDummy >>= 1;
  } else if (BusWidth == 0x144u) {
    NumCyclesDummy >>= 2;
  }
  if (NumCyclesDummy < MIN_NUM_CYCLES_DUMMY) {
    NumCyclesDummy = MIN_NUM_CYCLES_DUMMY;
  }
  if (NumCyclesDummy > MAX_NUM_CYCLES_DUMMY) {
    NumCyclesDummy = MAX_NUM_CYCLES_DUMMY;
  }
  NumCyclesDummy -= 2;                          // The actual value written to register is smaller.
  QSPI_SFMSIC  = ReadCmd;
  QSPI_SFMSAC  = AddrMode;
  QSPI_SFMSMD |= 0
              | ReadMode
              ;
  QSPI_SFMSDC = 0
              | (NumCyclesDummy << SFMSDC_SFMDN_BIT)
              | (0xFFuL         << SFMSDC_SFMXD_BIT)
              ;
  QSPI_SFMCMD &= ~(1uL << SFMCMD_DCOM_BIT);     // Exit direct communication mode and enter memory-mapped mode.
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(BusWidth);

  ASSERT_BUS_WIDTH1(BusWidth);
  QSPI_SFMCOM = Cmd;                        // At the first access to QSPI_SFMCON the CS signal is set to low.
  QSPI_SFMCMD = 1uL << SFMCMD_DCOM_BIT;     // End the bus cycle by setting the CS signal to high.
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BusWidth);

  ASSERT_BUS_WIDTH1(BusWidth);
  QSPI_SFMCOM = Cmd;                        // At the first access to QSPI_SFMCON the CS signal is set to low.
  if (NumBytesPara) {
    do {
      QSPI_SFMCOM = *pPara++;
    } while (--NumBytesPara);
  }
  if (NumBytesData) {
    do {
      *pData++ = (U8)QSPI_SFMCOM;
    } while (--NumBytesData);
  }
  QSPI_SFMCMD = 1uL << SFMCMD_DCOM_BIT;     // End the bus cycle by setting the CS signal to high.
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BusWidth);

  ASSERT_BUS_WIDTH1(BusWidth);
  QSPI_SFMCOM = Cmd;                        // At the first access to QSPI_SFMCON the CS signal is set to low.
  if (NumBytesPara) {
    do {
      QSPI_SFMCOM = *pPara++;
    } while (--NumBytesPara);
  }
  if (NumBytesData) {
    if ((NumBytesData & 0x7uL) == 0) {      // Write 8 bytes at a time if possible.
      do {
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        QSPI_SFMCOM = *pData++;
        NumBytesData -= 8;
      } while (NumBytesData);
    } else {
      do {
        QSPI_SFMCOM = *pData++;
      } while (--NumBytesData);
    }
  }
  QSPI_SFMCMD = 1uL << SFMCMD_DCOM_BIT;     // End the bus cycle by setting the CS signal to high.
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_R7FA6M4_Renesas_RA6M4_EK = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
