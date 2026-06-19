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

File    : FS_NOR_HW_SPIFI_LPC54608_NXP_LPCXpresso54608.c
Purpose : Low-level flash driver for NXP SPIFI interface.
Literature:
  [1] UM10912 LPC546xx User manual
    (\\FILESERVER\Techinfo\Company\NXP\MCU\LPC54xx\UM10912_LPC546xx_UserManual_Rev2.1.pdf)
  [2] UM11035 LPCXpresso546x8/540xx Board User Manual
    (\\fileserver\Techinfo\Company\NXP\MCU\LPC54xx\EvalBoard\LPCXpresso54608\UM11035_LPC546xx_Xpresso_UserManual.pdf)
  [3] Schematic
    (\\fileserver\Techinfo\Company\NXP\MCU\LPC54xx\EvalBoard\LPCXpresso54608\LPC54608_Dev_brd_schematic_Rev-B.pdf)
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
#ifndef   FS_NOR_HW_SPIFI_CLK_HZ
  #define FS_NOR_HW_SPIFI_CLK_HZ          (180000000uL / 3)   // Frequency of the clock supplied to SPIFI unit.
#endif

#ifndef   FS_NOR_HW_SPIFI_DATA_ADDR
  #define FS_NOR_HW_SPIFI_DATA_ADDR       0x10000000          // This is the start address of the memory region used by the file system
                                                              // to read the data from the serial NOR flash device. The hardware layer
                                                              // performs a dummy read from this address when switching to memory mode
                                                              // in order to clean the caches. It should be set to the value passed
                                                              // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

#ifndef   FS_NOR_HW_RESET_DELAY_LOOPS
  #define FS_NOR_HW_RESET_DELAY_LOOPS     100000              // Number of software loops to wait for NOR device to reset
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS                0                   // Enables / disables the interrupt driver operation
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "RTOS.h"
  #include "LPC54xxx.h"
  #include "FS_OS.h"
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       SPIFI unit
*/
#define SPIFI_BASE_ADDR                           0x40080000
#define SPIFI_CTRL                                (*(volatile U32*)(SPIFI_BASE_ADDR + 0x00))    // SPIFI control register
#define SPIFI_CMD                                 (*(volatile U32*)(SPIFI_BASE_ADDR + 0x04))    // SPIFI command register
#define SPIFI_ADDR                                (*(volatile U32*)(SPIFI_BASE_ADDR + 0x08))    // SPIFI address register
#define SPIFI_IDATA                               (*(volatile U32*)(SPIFI_BASE_ADDR + 0x0C))    // SPIFI intermediate data register
#define SPIFI_CLIMIT                              (*(volatile U32*)(SPIFI_BASE_ADDR + 0x10))    // SPIFI cache limit register
#define SPIFI_DATA32                              (*(volatile U32*)(SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (32-bit access, 4 bytes at once)
#define SPIFI_DATA16                              (*(volatile U16*)(SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (16-bit access, 2 bytes at once)
#define SPIFI_DATA8                               (*(volatile U8*) (SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (8-bit access, 1 byte at once)
#define SPIFI_MCMD                                (*(volatile U32*)(SPIFI_BASE_ADDR + 0x18))    // SPIFI memory command register
#define SPIFI_STAT                                (*(volatile U32*)(SPIFI_BASE_ADDR + 0x1C))    // SPIFI status register

/*********************************************************************
*
*       System configuration
*/
#define SYSCON_BASE_ADDR                          0x40000000uL
#define SYSCON_PRESETCTRL0                        (*(volatile U32*)(SYSCON_BASE_ADDR + 0x100))
#define SYSCON_PRESETCTRLSET0                     (*(volatile U32*)(SYSCON_BASE_ADDR + 0x120))
#define SYSCON_PRESETCTRLCLR0                     (*(volatile U32*)(SYSCON_BASE_ADDR + 0x140))
#define SYSCON_AHBCLKCTRL0                        (*(volatile U32*)(SYSCON_BASE_ADDR + 0x200))
#define SYSCON_AHBCLKCTRLSET0                     (*(volatile U32*)(SYSCON_BASE_ADDR + 0x220))
#define SYSCON_AHBCLKCTRLCLR0                     (*(volatile U32*)(SYSCON_BASE_ADDR + 0x240))
#define SYSCON_SPIFICLKSEL                        (*(volatile U32*)(SYSCON_BASE_ADDR + 0x2A0))
#define SYSCON_SPIFICLKDIV                        (*(volatile U32*)(SYSCON_BASE_ADDR + 0x390))

/*********************************************************************
*
*       System control unit
*/
#define IOCON_BASE_ADDR                           0x40001000uL
#define PIO0_23                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x05C))   // SPIFI_CS
#define PIO0_24                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x060))   // SPIFI_IO0
#define PIO0_25                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x064))   // SPIFI_IO1
#define PIO0_26                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x068))   // SPIFI_CLK
#define PIO0_27                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x06C))   // SPIFI_IO3
#define PIO0_28                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x070))   // SPIFI_IO2
#define PIO2_12                                   (*(volatile U32*)(IOCON_BASE_ADDR + 0x130))   // SPIFI_RST

/*********************************************************************
*
*       GPIO unit
*/
#define GPIO_BASE_ADDR                            0x4008C000uL
#define GPIO_DIR2                                 (*(volatile U32*)(GPIO_BASE_ADDR + 0x2008))   // Direction registers port 2
#define GPIO_SET2                                 (*(volatile U32*)(GPIO_BASE_ADDR + 0x2208))   // Set register for port 2
#define GPIO_CLR2                                 (*(volatile U32*)(GPIO_BASE_ADDR + 0x2288))   // Clear register for port 2

/*********************************************************************
*
*       SPIFI command register
*/
#define CMD_BITPOS_BIT                            0
#define CMD_BITPOS_MASK                           0x7uL
#define CMD_BITVAL_BIT                            3
#define CMD_BITVAL_MASK                           0x1uL
#define CMD_DATALEN_BIT                           0
#define CMD_DATALEN_MASK                          0x3FFFuL
#define CMD_POLL_BIT                              14
#define CMD_DOUT_BIT                              15
#define CMD_INTLEN_BIT                            16
#define CMD_INTLEN_MASK                           0x7uL
#define CMD_FIELDFORM_BIT                         19
#define CMD_FIELDFORM_ALL_SERIAL                  0x0uL
#define CMD_FIELDFORM_QUAD_DUAL_DATA              0x1uL
#define CMD_FIELDFORM_SERIAL_OPCODE               0x2uL
#define CMD_FIELDFORM_ALL_QUAD_DUAL               0x3uL
#define CMD_FRAMEFORM_BIT                         21
#define CMD_FRAMEFORM_OPCODE                      0x1uL
#define CMD_FRAMEFORM_OPCODE_1BYTE                0x2uL
#define CMD_FRAMEFORM_OPCODE_2BYTES               0x3uL
#define CMD_FRAMEFORM_OPCODE_3BYTES               0x4uL
#define CMD_FRAMEFORM_OPCODE_4BYTES               0x5uL
#define CMD_OPCODE_BIT                            24
#define CMD_OPCODE_MASK                           0xFFuL

/*********************************************************************
*
*       SPIFI control register
*/
#define CTRL_TIMEOUT_BIT                          0
#define CTRL_CSHIGH_BIT                           16
#define CTRL_D_PRFTCH_DIS_BIT                     21
#define CTRL_MODE3_BIT                            23
#define CTRL_INTEN_BIT                            22
#define CTRL_PRFTCH_DIS_BIT                       27
#define CTRL_DUAL_BIT                             28
#define CTRL_FBCLK_BIT                            30

/*********************************************************************
*
*       SPIFI status register
*/
#define STAT_MCINIT_BIT                           0
#define STAT_CMD_BIT                              1
#define STAT_RESET_BIT                            4
#define STAT_INTRQ_BIT                            5

/*********************************************************************
*
*       Port I/O settings
*/
#define PIO_FUNC_BIT                              0
#define PIO_MODE_BIT                              4
#define PIO_MODE_PULLUP                           2u
#define PIO_INVERT_BIT                            7
#define PIO_DIGIMODE_BIT                          8
#define PIO_FILTEROFF_BIT                         9
#define PIO_SLEW_BIT                              10
#define PIO_OD_BIT                                11

/*********************************************************************
*
*       Misc. defines
*/
#define SPIFI_PRIO                                15
#define AHBCLKCTRL0_SPIFI_BIT                     10
#define AHBCLKCTRL0_IOCON_BIT                     13
#define PRESETCTRL_SPIFI_RST_BIT                  10
#define SPIFICLKSEL_SEL_BIT                       0
#define SPIFICLKSEL_SEL_MAIN_CLK                  0
#define SPIFICLKSEL_SEL_PLL_CLK                   1
#define SDIOCLKDIV_DIV_BIT                        0
#define NOR_RESET_BIT                             12
#define WAIT_TIMEOUT                              1000      // milliseconds
#define CYCLES_PER_MS                             20000     // Blocks the execution for about 1ms on a CPU running at 180 Mhz.
#define WAIT_TIMEOUT_CYCLES                       (WAIT_TIMEOUT * CYCLES_PER_MS)

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _GetFieldForm
*
*  Function description
*    Determines the value to be stored in the FIELDFORM field
*    of the SPIFI_CMD and SPIFI_MCMD registers. It also returns
*    the value of the SPIFI_CTRL.DUAL field.
*/
static U32 _GetFieldForm(U16 BusWidth, int * pIsDual) {
  U32 FieldForm;
  int IsDual;

  IsDual = 0;
  if (BusWidth == 0x112u) {
    FieldForm = CMD_FIELDFORM_QUAD_DUAL_DATA;   // Data field is dual other fields are serial.
    IsDual    = 1;
  } else if (BusWidth == 0x114u) {
    FieldForm = CMD_FIELDFORM_QUAD_DUAL_DATA;   // Data field is quad other fields are serial.
  } else if (BusWidth == 0x122u) {
    FieldForm = CMD_FIELDFORM_SERIAL_OPCODE;    // Opcode field is serial. Other fields are dual.
    IsDual    = 1;
  } else if (BusWidth == 0x144u) {
    FieldForm = CMD_FIELDFORM_SERIAL_OPCODE;    // Opcode field is serial. Other fields are quad.
  } else if (BusWidth == 0x444u) {
    FieldForm = CMD_FIELDFORM_ALL_QUAD_DUAL;    // Opcode field is serial. Other fields are quad.
  } else {
    FieldForm = CMD_FIELDFORM_ALL_SERIAL;       // All fields of the command are serial.
  }
  if (pIsDual) {
    *pIsDual = IsDual;
  }
  return FieldForm;
}

/*********************************************************************
*
*       _GetFrameForm
*
*  Function description
*    Determines the value to be stored in the FAMEFORM field
*    of the SPIFI_CMD and SPIFI_MCMD registers.
*/
static U32 _GetFrameForm(unsigned NumBytesAddr) {
  U32 FrameForm;

  if (NumBytesAddr == 1) {
    FrameForm = CMD_FRAMEFORM_OPCODE_1BYTE;     // Opcode + 1 address byte.
  } else if (NumBytesAddr == 2) {
    FrameForm = CMD_FRAMEFORM_OPCODE_2BYTES;    // Opcode + 2 address bytes.
  } else if (NumBytesAddr == 3) {
    FrameForm = CMD_FRAMEFORM_OPCODE_3BYTES;    // Opcode + 3 address bytes.
  } else if (NumBytesAddr == 4) {
    FrameForm = CMD_FRAMEFORM_OPCODE_4BYTES;    // Opcode + 4 address bytes.
  } else {
    FrameForm = CMD_FRAMEFORM_OPCODE;           // Opcode, no address bytes.
  }
  return FrameForm;
}

/*********************************************************************
*
*       _Reset
*/
static void _Reset(void) {
  SPIFI_STAT = (1uL << STAT_RESET_BIT);               // Reset the SPIFI controller. The controller is put into SPI mode.
  while (1) {
    if ((SPIFI_STAT & ((1uL << STAT_CMD_BIT)   |
                       (1uL << STAT_RESET_BIT) |
                       (1uL << STAT_MCINIT_BIT))) == 0) {
      break;                                          // Wait until the SPIFI controller is ready again.
    }
  }
}

/*********************************************************************
*
*      Public code
*
**********************************************************************
*/

#if FS_NOR_HW_USE_OS

/*********************************************************************
*
*       SPIFI0_IRQHandler
*/
void SPIFI0_IRQHandler(void);
void SPIFI0_IRQHandler(void) {
  OS_EnterInterrupt();                          // Tell embOS that we enter the ISR.
  SPIFI_STAT |= 1uL << STAT_INTRQ_BIT;          // Prevent other interrupts from occurring.
  FS_X_OS_Signal();
  OS_LeaveInterrupt();                          // Tell embOS that we leage the ISR.
}

#endif

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
  unsigned NumLoops;
  unsigned TimeOut;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Enable the clock for the IO controller if required.
  //
  if ((SYSCON_AHBCLKCTRL0 & (1uL << AHBCLKCTRL0_IOCON_BIT)) == 0) {
    SYSCON_AHBCLKCTRLSET0 = 1uL << AHBCLKCTRL0_IOCON_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    while (1) {
      if ((SYSCON_AHBCLKCTRL0 & (1uL << AHBCLKCTRL0_IOCON_BIT)) != 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  //
  // Disable the SPIFI clock if required.
  //
  if ((SYSCON_AHBCLKCTRL0 & (1uL << AHBCLKCTRL0_SPIFI_BIT)) != 0) {
    SYSCON_AHBCLKCTRLCLR0 = 1uL << AHBCLKCTRL0_SPIFI_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    while (1) {
      if ((SYSCON_AHBCLKCTRL0 & (1uL << AHBCLKCTRL0_SPIFI_BIT)) == 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  //
  // Reset SPIFI.
  //
  SYSCON_PRESETCTRLSET0 = 1uL << PRESETCTRL_SPIFI_RST_BIT;
  SYSCON_PRESETCTRLCLR0 = 1uL << PRESETCTRL_SPIFI_RST_BIT;
  //
  // Select the clock for SPIFI and configure the divisor.
  //
  SYSCON_SPIFICLKSEL = SPIFICLKSEL_SEL_MAIN_CLK << SPIFICLKSEL_SEL_BIT;
  SYSCON_SPIFICLKDIV = 2uL << SDIOCLKDIV_DIV_BIT;       // Do not forget to update FS_NOR_HW_SPIFI_CLK_HZ.
  //
  // Enable the SPIFI clock.
  //
  SYSCON_AHBCLKCTRLSET0 = 1uL << AHBCLKCTRL0_SPIFI_BIT;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  while (1) {
    if ((SYSCON_AHBCLKCTRL0 & (1uL << AHBCLKCTRL0_SPIFI_BIT)) != 0) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
  PIO0_23 = 0  // SPIFI_CS
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;
  PIO0_26 = 0  // SPIFI_CLK
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;
  PIO0_24 = 0  // SPIFI_IO0
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;
  PIO0_25 = 0  // SPIFI_IO1
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;
  PIO0_28 = 0  // SPIFI_IO2
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;
  PIO0_27 = 0  // SPIFI_IO3
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          | (6uL << PIO_FUNC_BIT)
          ;

  //
  // Reset pin. Present only on 16-pin devices.
  //
  PIO2_12 = 0
          | (1uL << PIO_SLEW_BIT)
          | (1uL << PIO_FILTEROFF_BIT)
          | (1uL << PIO_DIGIMODE_BIT)
          ;
  GPIO_DIR2 |= 1uL << NOR_RESET_BIT;
  //
  // Reset the NOR device.
  //
  GPIO_CLR2 = 1uL << NOR_RESET_BIT;                 // Assert reset signal (active low).
  NumLoops = FS_NOR_HW_RESET_DELAY_LOOPS;
  do {
    ;
  } while (--NumLoops);
  GPIO_SET2 = 1uL << NOR_RESET_BIT;                 // De-assert reset signal (active low).
#if FS_NOR_HW_USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(SPIFI0_IRQn);
  NVIC_SetPriority(SPIFI0_IRQn, SPIFI_PRIO);
#endif
  //
  // Configure the SPIFI unit.
  //
  SPIFI_CTRL = 0
             | (0xFFFFuL << CTRL_TIMEOUT_BIT)       // Use the maximum timeout.
             | (0xFuL    << CTRL_CSHIGH_BIT)        // Set minimum CS high time to maximum value.
             | (0x1uL    << CTRL_D_PRFTCH_DIS_BIT)  // Disable memory prefetches.
             | (0x1uL    << CTRL_MODE3_BIT)         // CLK idle mode is high.
             | (0x1uL    << CTRL_PRFTCH_DIS_BIT)    // Disables prefetching of cache lines.
             | (0x1uL    << CTRL_FBCLK_BIT)         // Read data is sampled using a feedback clock from the SCK pin.
             ;
#if FS_NOR_HW_USE_OS
  //
  // Clear interrupt flag.
  //
  SPIFI_STAT = 1uL << STAT_INTRQ_BIT;
  NVIC_EnableIRQ(SPIFI0_IRQn);
#endif
  return FS_NOR_HW_SPIFI_CLK_HZ;
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
  FS_USE_PARA(Unit);            // This device has only one HW unit.
  _Reset();
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
  U32 FieldForm;
  U32 FrameForm;
  U32 OpCode;
  U32 IntLen;
  int IsDual;
  U32 MCmdReg;
  U32 v;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Determine how the data transferred is sent (serial, dual or quad)
  // and how many address bytes to send.
  //
  IsDual    = 0;
  FieldForm = _GetFieldForm(BusWidth, &IsDual);
  FrameForm = _GetFrameForm(NumBytesAddr);
  IntLen    = NumBytesDummy & CMD_INTLEN_MASK;
  OpCode    = ReadCmd & CMD_OPCODE_MASK;
  MCmdReg   = 0
            | (IntLen    << CMD_INTLEN_BIT)
            | (FieldForm << CMD_FIELDFORM_BIT)
            | (FrameForm << CMD_FRAMEFORM_BIT)
            | (OpCode    << CMD_OPCODE_BIT)
            ;
  //
  // Configure the SPIFI controller to work in memory-mapped mode.
  //
  v = SPIFI_CTRL;
  if (IsDual) {
    v |=   1uL << CTRL_DUAL_BIT;
  } else {
    v &= ~(1uL << CTRL_DUAL_BIT);
  }
  SPIFI_CTRL = v;
  SPIFI_MCMD = MCmdReg;
  //
  // Wait until MCMD is written
  //
  while (1) {
    if (SPIFI_STAT & (1uL << STAT_MCINIT_BIT)) {
      break;
    }
  }
  //
  // Perform dummy read to bring controller into sync and invalidate all caches etc.
  // It seems to be necessary but not documented in the datasheet.
  //
  *(volatile U32*)FS_NOR_HW_SPIFI_DATA_ADDR;
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
  U32 FieldForm;
  int IsDual;
  U32 FrameForm;
  U32 CmdReg;
  U32 v;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  IsDual    = 0;
  FieldForm = _GetFieldForm(BusWidth, &IsDual);
  FrameForm = _GetFrameForm(0);                 // 0 - No address bytes are sent.
  CmdReg    = 0
            | (FieldForm << CMD_FIELDFORM_BIT)  // Configure the transfer mode.
            | (FrameForm << CMD_FRAMEFORM_BIT)  // Send only the command byte.
            | ((U32)Cmd  << CMD_OPCODE_BIT)     // Command code.
            ;
  v = SPIFI_CTRL;
  if (IsDual) {
    v |=   1uL << CTRL_DUAL_BIT;
  } else {
    v &= ~(1uL << CTRL_DUAL_BIT);
  }
  SPIFI_CTRL = v;
  SPIFI_CMD  = CmdReg;
  //
  // Wait until nCS is set high, indicating that the command has been completed.
  //
  while (1) {
    if ((SPIFI_STAT & (1uL << STAT_CMD_BIT)) == 0) {
      break;
    }
  }
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
  U32      DataLen;
  U32      AddrReg;
  U32      IDataReg;
  U32      CmdReg;
  U32      FieldForm;
  int      IsDual;
  U32      FrameForm;
  U32      IntLen;
  unsigned NumBytes;
  U32      v;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  IsDual    = 0;
  FieldForm = _GetFieldForm(BusWidth, &IsDual);
  FrameForm = _GetFrameForm(NumBytesAddr);
  IntLen    = 0;
  DataLen   = NumBytesData & CMD_DATALEN_MASK;
  AddrReg   = 0;
  IDataReg  = 0;
  //
  // Encode the address.
  //
  if (NumBytesAddr) {
    NumBytes = NumBytesAddr;
    do {
      AddrReg <<= 8;
      AddrReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  //
  // Encode the dummy and mode bytes.
  //
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    IntLen   = NumBytes;
    do {
      IDataReg <<= 8;
      IDataReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  CmdReg = 0
         | (DataLen   << CMD_DATALEN_BIT)
         | (IntLen    << CMD_INTLEN_BIT)
         | (FieldForm << CMD_FIELDFORM_BIT)
         | (FrameForm << CMD_FRAMEFORM_BIT)
         | ((U32)Cmd  << CMD_OPCODE_BIT)
         ;
  //
  // Set dual/quad mode.
  //
  v = SPIFI_CTRL;
  if (IsDual) {
    v |=   1uL << CTRL_DUAL_BIT;
  } else {
    v &= ~(1uL << CTRL_DUAL_BIT);
  }
  SPIFI_CTRL = v;
  //
  // Execute the command.
  //
  SPIFI_ADDR  = AddrReg;
  SPIFI_IDATA = IDataReg;
  SPIFI_CMD   = CmdReg;
  //
  // Read data from NOR flash.
  //
  if (NumBytesData) {
    do {
      *pData++ = SPIFI_DATA8;
    } while (--NumBytesData);
  }
  //
  // Wait until nCS is set high, indicating that the command has been completed.
  //
  while (1) {
    if ((SPIFI_STAT & (1uL << STAT_CMD_BIT)) == 0) {
      break;
    }
  }
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
  U32      DataLen;
  U32      AddrReg;
  U32      IDataReg;
  U32      FieldForm;
  int      IsDual;
  U32      FrameForm;
  U32      IntLen;
  unsigned NumBytesAtOnce;
  unsigned NumBytes;
  U32      v;
  U32      CmdReg;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  IsDual    = 0;
  FieldForm = _GetFieldForm(BusWidth, &IsDual);
  FrameForm = _GetFrameForm(NumBytesAddr);
  IntLen    = 0;
  DataLen   = NumBytesData & CMD_DATALEN_MASK;
  AddrReg   = 0;
  IDataReg  = 0;
  //
  // Encode the address.
  //
  if (NumBytesAddr) {
    NumBytes = NumBytesAddr;
    do {
      AddrReg <<= 8;
      AddrReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  //
  // Encode the dummy and mode bytes.
  //
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    IntLen   = NumBytes;
    do {
      IDataReg <<= 8;
      IDataReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  CmdReg = 0
         | (DataLen   << CMD_DATALEN_BIT)
         | (1uL       << CMD_DOUT_BIT)     // Transfer data to NOR flash
         | (IntLen    << CMD_INTLEN_BIT)
         | (FieldForm << CMD_FIELDFORM_BIT)
         | (FrameForm << CMD_FRAMEFORM_BIT)
         | ((U32)Cmd  << CMD_OPCODE_BIT)
         ;
  //
  // Set dual/quad mode.
  //
  v = SPIFI_CTRL;
  if (IsDual) {
    v |=   1uL << CTRL_DUAL_BIT;
  } else {
    v &= ~(1uL << CTRL_DUAL_BIT);
  }
  SPIFI_CTRL = v;
  //
  // Execute the command.
  //
  SPIFI_ADDR  = AddrReg;
  SPIFI_IDATA = IDataReg;
  SPIFI_CMD   = CmdReg;
  //
  // Wait for the command to start execution.
  //
  if (NumBytesData) {
    while (1) {
      if (SPIFI_STAT & (1uL << STAT_CMD_BIT)) {
        break;
      }
    }
  }
  //
  // Write data to NOR flash.
  //
  if (NumBytesData) {
    do {
      NumBytesAtOnce = NumBytesData >= 4 ? 4 : 1;
      if (NumBytesAtOnce == 4) {
        v = 0
          | (U32)*pData
          | ((U32)(*(pData + 1)) << 8)
          | ((U32)(*(pData + 2)) << 16)
          | ((U32)(*(pData + 3)) << 24)
          ;
        SPIFI_DATA32 = v;
      } else {
        SPIFI_DATA8 = *pData;
      }
      NumBytesData -= NumBytesAtOnce;
      pData        += NumBytesAtOnce;
    } while (NumBytesData);
  }
  //
  // Wait until nCS is set high, indicating that the command has been completed.
  //
  while (1) {
    if ((SPIFI_STAT & (1uL << STAT_CMD_BIT)) == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _HW_Poll
*
*  Function description
*    HW layer function. Sends a command repeatedly and checks the
*    response for a specified condition.
*
*  Return value
*    > 0    Timeout occurred.
*    ==0    OK, bit set to specified value.
*    < 0    Feature not supported.
*
*  Additional information
*    The function executes periodically a command and waits
*    until specified bit in the response returned by the NOR flash
*    is set to a value specified by BitValue. The position of the bit
*    that has to be checked is specified by BitPos where 0 is the
*    position of the least significant bit in the byte.
*/
static int _HW_Poll(U8 Unit, U8 Cmd, U8 BitPos, U8 BitValue, U32 Delay, U32 TimeOut_ms, U16 BusWidth) {
  int r;

  FS_USE_PARA(Unit);
  FS_USE_PARA(Delay);
  r = -1;                 // Set to indicate that the feature is not supported.
#if FS_NOR_HW_USE_OS
  {
    U32 AddrReg;
    U32 IDataReg;
    U32 CmdReg;
    U32 FieldForm;
    int IsDual;
    U32 FrameForm;
    U32 v;

    //
    // Fill local variables.
    //
    IsDual    = 0;
    FieldForm = _GetFieldForm(BusWidth, &IsDual);
    FrameForm = _GetFrameForm(0);                 // The command does not require any address bytes.
    AddrReg   = 0;
    IDataReg  = 0;
    CmdReg = 0
           | ((BitValue & CMD_BITVAL_MASK) << CMD_BITVAL_BIT)
           | ((BitPos & CMD_BITPOS_MASK)   << CMD_BITPOS_BIT)
           | (1uL                          << CMD_POLL_BIT)
           | (FieldForm                    << CMD_FIELDFORM_BIT)
           | (FrameForm                    << CMD_FRAMEFORM_BIT)
           | (Cmd                          << CMD_OPCODE_BIT)
           ;
    //
    // Set dual/quad mode.
    //
    v = SPIFI_CTRL;
    if (IsDual) {
      v |=   1uL << CTRL_DUAL_BIT;
    } else {
      v &= ~(1uL << CTRL_DUAL_BIT);
    }
    SPIFI_CTRL = v;
    //
    // Execute the command.
    //
    SPIFI_STAT  = 1uL << STAT_INTRQ_BIT;    // Clear the interrupt flag.
    SPIFI_CTRL |= 1uL << CTRL_INTEN_BIT;    // Enable interrupt.
    SPIFI_ADDR  = AddrReg;
    SPIFI_IDATA = IDataReg;
    SPIFI_CMD   = CmdReg;
    r = FS_X_OS_Wait(TimeOut_ms);
    SPIFI_CTRL &= ~(1uL << CTRL_INTEN_BIT); // Disable interrupt.
    if (r) {
      r = 1;                                // A timeout occurred.
    } else {
      r = 0;                                // Bit set to the requested value.
    }
    _Reset();                               // Cancel the polling operation.
    //
    // Wait until nCS is set high, indicating that the command has been completed.
    //
    while (1) {
      if ((SPIFI_STAT & (1uL << STAT_CMD_BIT)) == 0) {
        break;
      }
    }
  }
#else
  FS_USE_PARA(Cmd);
  FS_USE_PARA(BitPos);
  FS_USE_PARA(BitValue);
  FS_USE_PARA(Delay);
  FS_USE_PARA(TimeOut_ms);
  FS_USE_PARA(BusWidth);
#endif
  return r;
}

/*********************************************************************
*
*       _HW_Delay
*
*  Function description
*    HW layer function. Blocks the execution for the specified number
*    of milliseconds.
*/
static int _HW_Delay(U8 Unit, U32 ms) {
  int r;

  FS_USE_PARA(Unit);
  r = -1;                 // Set to indicate that the feature is not supported.
#if FS_NOR_HW_USE_OS
  if (ms) {
    FS_X_OS_Delay(ms);
    r = 0;
  }
#else
  FS_USE_PARA(ms);
#endif
  return r;
}

/*********************************************************************
*
*       _HW_Lock
*
*  Function description
*    HW layer function. Requests exclusive access to SPI bus.
*/
static void _HW_Lock(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Unlock
*
*  Function description
*    HW layer function. Releases the exclusive access of SPI bus.
*/
static void _HW_Unlock(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_LPC54608_NXP_LPCXpresso54608 = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
  _HW_Poll,
  _HW_Delay,
  _HW_Lock,
  _HW_Unlock,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
