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

File    : NOR_PHY_SPIFI_NXP_LPC4322_SEGGER_QSPIFI_Test_Board.c
Purpose : Low-level flash driver for NXP SPIFI interface.
Literature:
  [1] UM10503 LPC43xx ARM Cortex-M4/M0 multi-core microcontroller
      (\\fileserver\Techinfo\Company\NXP\MCU\LPC43xx\UserManual_LPC43xx_UM10503_Rev1.90_150218.pdf)
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
#define SPIFI_CLK_HZ          60000000uL    // Frequency of the clock supplied to SPIFI unit.
#define SPIFI_DATA_ADDR       0x80000000    // This is the start address of the memory region used by the file system
                                            // to read the data from the serial NOR flash device. The hardware layer
                                            // performs a dummy read from this address when switching to memory mode
                                            // in order to clean the caches. It should be set to the value passed
                                            // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#define RESET_DELAY_LOOPS     100000        // Number of software loops to wait for NOR device to reset
#define USE_OS                0             // Enables / disables the interrupt driver operation

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if USE_OS
  #include "RTOS.h"
  #include "lpc43xx.h"
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
#define SPIFI_BASE_ADDR       0x40003000
#define SPIFI_CTRL            (*(volatile U32*)(SPIFI_BASE_ADDR + 0x00))    // SPIFI control register
#define SPIFI_CMD             (*(volatile U32*)(SPIFI_BASE_ADDR + 0x04))    // SPIFI command register
#define SPIFI_ADDR            (*(volatile U32*)(SPIFI_BASE_ADDR + 0x08))    // SPIFI address register
#define SPIFI_IDATA           (*(volatile U32*)(SPIFI_BASE_ADDR + 0x0C))    // SPIFI intermediate data register
#define SPIFI_CLIMIT          (*(volatile U32*)(SPIFI_BASE_ADDR + 0x10))    // SPIFI cache limit register
#define SPIFI_DATA32          (*(volatile U32*)(SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (32-bit access, 4 bytes at once)
#define SPIFI_DATA16          (*(volatile U16*)(SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (16-bit access, 2 bytes at once)
#define SPIFI_DATA8           (*(volatile U8*) (SPIFI_BASE_ADDR + 0x14))    // SPIFI data register (8-bit access, 1 byte at once)
#define SPIFI_MCMD            (*(volatile U32*)(SPIFI_BASE_ADDR + 0x18))    // SPIFI memory command register
#define SPIFI_STAT            (*(volatile U32*)(SPIFI_BASE_ADDR + 0x1C))    // SPIFI status register

/*********************************************************************
*
*       Clock generation unit
*/
#define CGU_BASE_ADDR         0x40050000
#define IDIVA_CTRL            (*(volatile U32*)(CGU_BASE_ADDR + 0x0048))
#define IDIVB_CTRL            (*(volatile U32*)(CGU_BASE_ADDR + 0x004C))
#define BASE_SPIFI_CLK        (*(volatile U32*)(CGU_BASE_ADDR + 0x0070))

/*********************************************************************
*
*       Clock control unit 1
*/
#define CCU1_BASE_ADDR        0x40051000
#define CLK_SPIFI_CFG         (*(volatile U32*)(CCU1_BASE_ADDR + 0x0300))
#define CLK_SPIFI_STAT        (*(volatile U32*)(CCU1_BASE_ADDR + 0x0304))

/*********************************************************************
*
*       System control unit
*/
#define SCU_BASE_ADDR         0x40086000
#define SFSP3_2               (*(volatile U32*)(SCU_BASE_ADDR + 0x188))     // Pin configuration register for pin P3_2
#define SFSP3_3               (*(volatile U32*)(SCU_BASE_ADDR + 0x18C))     // Pin configuration register for pin P3_3
#define SFSP3_4               (*(volatile U32*)(SCU_BASE_ADDR + 0x190))     // Pin configuration register for pin P3_4
#define SFSP3_5               (*(volatile U32*)(SCU_BASE_ADDR + 0x194))     // Pin configuration register for pin P3_5
#define SFSP3_6               (*(volatile U32*)(SCU_BASE_ADDR + 0x198))     // Pin configuration register for pin P3_6
#define SFSP3_7               (*(volatile U32*)(SCU_BASE_ADDR + 0x19C))     // Pin configuration register for pin P3_7
#define SFSP3_8               (*(volatile U32*)(SCU_BASE_ADDR + 0x1A0))     // Pin configuration register for pin P3_8

/*********************************************************************
*
*       GPIO unit
*/
#define GPIO_BASE_ADDR        0x400F4000
#define GPIO_DIR5             (*(volatile U32*)(GPIO_BASE_ADDR + 0x2014))   // Direction registers port 5
#define GPIO_SET5             (*(volatile U32*)(GPIO_BASE_ADDR + 0x2214))   // Set register for port 5
#define GPIO_CLR5             (*(volatile U32*)(GPIO_BASE_ADDR + 0x2294))   // Clear register for port 5

/*********************************************************************
*
*       SPIFI command register
*/
#define CMD_BITPOS_BIT                0
#define CMD_BITPOS_MASK               0x7uL
#define CMD_BITVAL_BIT                3
#define CMD_BITVAL_MASK               0x1uL
#define CMD_DATALEN_BIT               0
#define CMD_DATALEN_MASK              0x3FFFuL
#define CMD_POLL_BIT                  14
#define CMD_DOUT_BIT                  15
#define CMD_INTLEN_BIT                16
#define CMD_INTLEN_MASK               0x7uL
#define CMD_FIELDFORM_BIT             19
#define CMD_FIELDFORM_ALL_SERIAL      0x0uL
#define CMD_FIELDFORM_QUAD_DUAL_DATA  0x1uL
#define CMD_FIELDFORM_SERIAL_OPCODE   0x2uL
#define CMD_FIELDFORM_ALL_QUAD_DUAL   0x3uL
#define CMD_FRAMEFORM_BIT             21
#define CMD_FRAMEFORM_OPCODE          0x1uL
#define CMD_FRAMEFORM_OPCODE_1BYTE    0x2uL
#define CMD_FRAMEFORM_OPCODE_2BYTES   0x3uL
#define CMD_FRAMEFORM_OPCODE_3BYTES   0x4uL
#define CMD_FRAMEFORM_OPCODE_4BYTES   0x5uL
#define CMD_OPCODE_BIT                24
#define CMD_OPCODE_MASK               0xFFuL

/*********************************************************************
*
*       SPIFI control register
*/
#define CTRL_TIMEOUT_BIT              0
#define CTRL_CSHIGH_BIT               16
#define CTRL_D_PRFTCH_DIS_BIT         21
#define CTRL_MODE3_BIT                23
#define CTRL_INTEN_BIT                22
#define CTRL_PRFTCH_DIS_BIT           27
#define CTRL_DUAL_BIT                 28
#define CTRL_FBCLK_BIT                30

/*********************************************************************
*
*       SPIFI status register
*/
#define STAT_MCINIT_BIT               0
#define STAT_CMD_BIT                  1
#define STAT_RESET_BIT                4
#define STAT_INTRQ_BIT                5

/*********************************************************************
*
*       Misc. defines
*/
#define CGU_IDIV_BIT                  2
#define CGU_AUTOBLOCK_BIT             11
#define CGU_CLK_SEL_BIT               24
#define SFS_MODE_BIT                  0
#define SFS_EPUN_BIT                  4
#define SFS_EHS_BIT                   5
#define SFS_EZI_BIT                   6
#define SFS_ZIF_BIT                   7
#define CLK_STAT_RUN_BIT              0
#define CLK_CFG_RUN_BIT               0
#define NOR_RESET_BIT                 9
#define SPIFI_PRIO                    15

/*********************************************************************
*
*       ASSERT_IS_LOCKED
*/
#if FS_SUPPORT_TEST
  #define ASSERT_IS_LOCKED()                  \
    if (_LockCnt != 1) {                      \
      FS_X_PANIC(FS_ERRCODE_INVALID_USAGE);   \
    }
#else
  #define ASSERT_IS_LOCKED()
#endif

/*********************************************************************
*
*       IF_TEST
*/
#if FS_SUPPORT_TEST
  #define IF_TEST(Expr)       Expr
#else
  #define IF_TEST(Expr)
#endif

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
#if FS_SUPPORT_TEST
  static U8 _LockCnt = 0;      // Just to test if the Lock()/Unlock function are called correctly.
#endif

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
*       _GetFieldFormEx
*
*  Function description
*    Same functionality as _GetFieldForm() with BusWidth not encoded.
*/
static U32 _GetFieldFormEx(U8 BusWidth, int * pIsDual) {
  U32 FieldForm;
  int IsDual;

  IsDual = 0;
  if (BusWidth == 1) {
    FieldForm = CMD_FIELDFORM_ALL_SERIAL;       // All fields of the command are serial.
  } else if (BusWidth == 2) {
    FieldForm = CMD_FIELDFORM_ALL_QUAD_DUAL;    // All fields are dual.
    IsDual = 1;
  } else if (BusWidth == 4) {
    FieldForm = CMD_FIELDFORM_ALL_QUAD_DUAL;    // All fields are quad.
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

#if USE_OS

/*********************************************************************
*
*       SPIFI_IRQHandler
*/
void SPIFI_IRQHandler(void);
void SPIFI_IRQHandler(void) {
  OS_EnterInterrupt();
  SPIFI_STAT |= 1uL << STAT_INTRQ_BIT;          // Prevent other interrupts from occurring.
  FS_X_OS_Signal();
  OS_LeaveInterrupt();
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

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Use IDIVA as clock for SPIFI. The input of IDIVA is PLL1.
  //
  IDIVA_CTRL     = 0
                 | (9uL << CGU_CLK_SEL_BIT)     // Use PLL1 as input
                 | (2uL << CGU_IDIV_BIT)        // Divide PLL1 by 3
                 | (1uL << CGU_AUTOBLOCK_BIT)
                 ;
  BASE_SPIFI_CLK = 0
                 | 12uL << CGU_CLK_SEL_BIT    // Use IDIVA as clock generator
                 | 1uL  << CGU_AUTOBLOCK_BIT
                 ;
  //
  // Enable the SPIFI clock if required.
  //
  if ((CLK_SPIFI_STAT & (1uL << CLK_STAT_RUN_BIT)) == 0) {
    CLK_SPIFI_CFG |= (1uL << CLK_CFG_RUN_BIT);
    while (1) {
      if (CLK_SPIFI_STAT & (1uL << CLK_CFG_RUN_BIT)) {
        ;
      }
    }
  }
  //
  // Clock pin.
  //
  SFSP3_3 = 0
          | (3uL << SFS_MODE_BIT)
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EHS_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  //
  // Data pins.
  //
  SFSP3_4 = 0
          | (3uL << SFS_MODE_BIT)
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  SFSP3_5 = 0
          | (3uL << SFS_MODE_BIT)
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  SFSP3_6 = 0
          | (3uL << SFS_MODE_BIT)
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  SFSP3_7 = 0
          | (3uL << SFS_MODE_BIT)
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  //
  // Chip select pin.
  //
  SFSP3_8 = 0
          | (3uL << SFS_MODE_BIT)                   // Controlled by this HW layer.
          | (1uL << SFS_EPUN_BIT)
          ;
  //
  // Reset pin. Present only on 16-pin devices.
  //
  SFSP3_2 = 0
          | (4uL << SFS_MODE_BIT)                   // Controlled by this HW layer.
          | (1uL << SFS_EPUN_BIT)
          ;
  GPIO_DIR5 |= 1uL << NOR_RESET_BIT;
  //
  // Reset the NOR device.
  //
  GPIO_CLR5 = 1uL << NOR_RESET_BIT;                 // Assert reset signal (active low).
  NumLoops = RESET_DELAY_LOOPS;
  do {
    ;
  } while (--NumLoops);
  GPIO_SET5 = 1uL << NOR_RESET_BIT;                 // De-assert reset signal (active low).
#if USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(SPIFI_IRQn);
  NVIC_SetPriority(SPIFI_IRQn, SPIFI_PRIO);
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
#if USE_OS
  //
  // Clear interrupt flag.
  //
  SPIFI_STAT = 1uL << STAT_INTRQ_BIT;
  NVIC_EnableIRQ(SPIFI_IRQn);
#endif
  return SPIFI_CLK_HZ;
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
  ASSERT_IS_LOCKED();
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

  ASSERT_IS_LOCKED();
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
  *(volatile U32*)SPIFI_DATA_ADDR;
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

  ASSERT_IS_LOCKED();
  FS_USE_PARA(Unit);    // This device has only one HW unit.
  IsDual    = 0;
  FieldForm = _GetFieldFormEx(BusWidth, &IsDual);
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

  ASSERT_IS_LOCKED();
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

  ASSERT_IS_LOCKED();
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

  ASSERT_IS_LOCKED();
  FS_USE_PARA(Unit);
  FS_USE_PARA(Delay);
  r = -1;                 // Set to indicate that the feature is not supported.
#if USE_OS
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
#if USE_OS
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
  IF_TEST(_LockCnt++);
  ASSERT_IS_LOCKED();
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
  ASSERT_IS_LOCKED();
  IF_TEST(_LockCnt--);
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_LPC4322_SEGGER_QSPIFI_Test_Board = {
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
