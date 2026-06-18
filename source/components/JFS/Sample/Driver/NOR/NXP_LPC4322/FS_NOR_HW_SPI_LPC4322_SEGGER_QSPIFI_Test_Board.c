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

File    : NOR_PHY_SPI_NXP_LPC4322_SEGGER_QSPIFI_Test_Board.c
Purpose : Low-level flash driver for NXP SPI interface.
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
#define PER_CLK_HZ            180000000uL     // Frequency of the clock supplied to SPI unit
#ifndef   SPI_CLK_HZ
  #define SPI_CLK_HZ          25000000uL      // Frequency of the clock supplied to NOR flash device
#endif
#define RESET_DELAY_LOOPS     100000          // Number of software loops to wait for NOR device to reset
#define USE_OS                0

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if USE_OS
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
*       SPI unit
*/
#define SPI_BASE_ADDR         0x40100000
#define SPI_CR                (*(volatile U32*)(SPI_BASE_ADDR + 0x00))    // SPI Control Register
#define SPI_SR                (*(volatile U32*)(SPI_BASE_ADDR + 0x04))    // SPI Status Register
#define SPI_DR                (*(volatile U32*)(SPI_BASE_ADDR + 0x08))    // SPI Data Register
#define SPI_CCR               (*(volatile U32*)(SPI_BASE_ADDR + 0x0C))    // SPI Clock Counter Register
#define SPI_TCR               (*(volatile U32*)(SPI_BASE_ADDR + 0x10))    // SPI Test Control register
#define SPI_TSR               (*(volatile U32*)(SPI_BASE_ADDR + 0x14))    // SPI Test Status register
#define SPI_INT               (*(volatile U32*)(SPI_BASE_ADDR + 0x1C))    // SPI Interrupt Flag

/*********************************************************************
*
*       Clock generation unit
*/
#define CGU_BASE_ADDR         0x40050000
#define BASE_SPI_CLK          (*(volatile U32*)(CGU_BASE_ADDR + 0x0074))

/*********************************************************************
*
*       Clock control unit 1
*/
#define CCU1_BASE_ADDR        0x40051000
#define CLK_SPI_CFG           (*(volatile U32*)(CCU1_BASE_ADDR + 0x0A00))
#define CLK_SPI_STAT          (*(volatile U32*)(CCU1_BASE_ADDR + 0x0A04))

/*********************************************************************
*
*       System control unit
*/
#define SCU_BASE_ADDR         0x40086000
#define SFSP3_2               (*(volatile U32*)(SCU_BASE_ADDR + 0x188))     // Pin configuration register for pin P3_2
#define SFSP3_3               (*(volatile U32*)(SCU_BASE_ADDR + 0x18C))     // Pin configuration register for pin P3_3
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
*       Misc. defines
*/
#define CGU_IDIV_BIT            2
#define CGU_AUTOBLOCK_BIT       11
#define CGU_CLK_SEL_BIT         24
#define SFS_MODE_BIT            0
#define SFS_EPUN_BIT            4
#define SFS_EHS_BIT             5
#define SFS_EZI_BIT             6
#define SFS_ZIF_BIT             7
#define CLK_STAT_RUN_BIT        0
#define CLK_CFG_RUN_BIT         0
#define NOR_CS_BIT              11
#define CR_MSTR_BIT             5
#define SR_SPIF                 7
#define CCR_COUNTER_MAX         0xFE        // The actual maximum value is 0xFF but it has to be a multiple of 2
#define NOR_RESET_BIT           9

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
*       ASSERT_IS_SELECTED
*/
#if FS_SUPPORT_TEST
  #define ASSERT_IS_SELECTED()                \
    if (_SelectCnt != 1) {                    \
      FS_X_PANIC(FS_ERRCODE_INVALID_USAGE);   \
    }
#else
  #define ASSERT_IS_SELECTED()
#endif

/*********************************************************************
*
*       ASSERT_IS_NOT_SELECTED
*/
#if FS_SUPPORT_TEST
  #define ASSERT_IS_NOT_SELECTED()            \
    if (_SelectCnt != 0) {                    \
      FS_X_PANIC(FS_ERRCODE_INVALID_USAGE);   \
    }
#else
  #define ASSERT_IS_NOT_SELECTED()
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
  static U8 _LockCnt   = 0;     // For testing if the Lock()/Unlock functions are called correctly.
  static U8 _SelectCnt = 0;     // For testing if the EnableCS()/DisableCS functions are called correctly.
#endif

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SetSpeed
*
*  Function description
*    Configures the speed of the clock supplied NOR flash device.
*
*  Return value
*    Frequency of the SPI clock in Hz.
*/
static U32 _SetSpeed(unsigned MaxFreq_Hz) {
  U32 Div;
  U32 Freq_Hz;

  //
  // According to [1] the clock divisor has to be >= 8 and a multiple of 2.
  //
  Div = 8;
  while (1) {
    Freq_Hz = PER_CLK_HZ / Div;
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    Div += 2;
    if (Div >= CCR_COUNTER_MAX) {
      Div = CCR_COUNTER_MAX;
      Freq_Hz = PER_CLK_HZ / Div;
      break;
    }
  }
  SPI_CCR = Div;
  return Freq_Hz;
}

/*********************************************************************
*
*       _TransferByte
*
*  Function description
*    Exchanges 8 bits of data with the NOR flash.
*
*  Parameters
*    Data       Data to be sent to NOR flash.
*
*  Return value
*    The data received from NOR flash
*/
static U8 _Transfer8Bits(U8 Data) {
  SPI_DR = Data;
  while (1) {
    if (SPI_SR & (1uL << SR_SPIF)) {
      break;
    }
  }
  Data = (U8)SPI_DR;
  return Data;
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
*    Frequency of the SPI clock in kHz.
*/
static int _HW_Init(U8 Unit) {
  unsigned Freq_Hz;
  unsigned NumLoops;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  BASE_SPI_CLK = 0
                 | 9uL  << CGU_CLK_SEL_BIT      // Use PLL1 as clock generator
                 | 1uL  << CGU_AUTOBLOCK_BIT
                 ;
  //
  // Enable the SPI clock if required.
  //
  if ((CLK_SPI_STAT & (1uL << CLK_STAT_RUN_BIT)) == 0) {
    CLK_SPI_CFG |= (1uL << CLK_CFG_RUN_BIT);
    while (1) {
      if (CLK_SPI_STAT & (1uL << CLK_CFG_RUN_BIT)) {
        ;
      }
    }
  }
  //
  // Clock pin (SPI_CLK).
  //
  SFSP3_3 = 0
          | (1uL << SFS_MODE_BIT)       // Controlled by SPI unit.
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EHS_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  //
  // Master In Slave Out (SPI_MISO).
  //
  SFSP3_6 = 0
          | (1uL << SFS_MODE_BIT)       // Controlled by SPI unit.
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  //
  // Master Out Slave In (SPI_MOSI).
  //
  SFSP3_7 = 0
          | (1uL << SFS_MODE_BIT)       // Controlled by SPI unit.
          | (1uL << SFS_EPUN_BIT)
          | (1uL << SFS_EZI_BIT)
          ;
  //
  // Chip select pin (SPI_CS).
  //
  SFSP3_8 = 0
          | (4uL << SFS_MODE_BIT)       // GPIO controlled by the this hardware layer.
          | (1uL << SFS_EPUN_BIT)
          ;
  GPIO_DIR5 |= 1uL << NOR_CS_BIT;
  GPIO_SET5  = 1uL << NOR_CS_BIT;       // Set to idle state.
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
  //
  // Configure the SPI unit.
  //
  SPI_CR = 0
         | (1uL << CR_MSTR_BIT)
         ;
  Freq_Hz = _SetSpeed(SPI_CLK_HZ);
  return (int)Freq_Hz / 1000;
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Activates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit   Device index
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  ASSERT_IS_LOCKED();
  ASSERT_IS_NOT_SELECTED();
  IF_TEST(_SelectCnt++);
  GPIO_CLR5 = 1uL << NOR_CS_BIT;      // The CS signal is active low.
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Deactivates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit   Device index
*/
static void _HW_DisableCS(U8 Unit) {
  ASSERT_IS_LOCKED();
  ASSERT_IS_SELECTED();
  IF_TEST(_SelectCnt--);
  FS_USE_PARA(Unit);
  GPIO_SET5 = 1uL << NOR_CS_BIT;      // The CS signal is active low.
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Reads a specified number of bytes from NOR flash to buffer.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be read
*/
static void _HW_Read(U8 Unit, U8 * pData, int NumBytes) {
  ASSERT_IS_LOCKED();
  ASSERT_IS_SELECTED();
  FS_USE_PARA(Unit);
  do {
    *pData++ = _Transfer8Bits(0xFF);
  } while (--NumBytes);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Writes a specified number of bytes from data buffer to NOR flash.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes) {
  ASSERT_IS_LOCKED();
  //
  // The delay operation is realized by sending dummy data with the CS disabled.
  // ASSERT_IS_SELECTED();
  //
  FS_USE_PARA(Unit);
  do {
    (void)_Transfer8Bits(*pData++);
  } while (--NumBytes);
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
  ASSERT_IS_NOT_SELECTED();
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
  ASSERT_IS_NOT_SELECTED();
  IF_TEST(_LockCnt--);
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_LPC4322_SEGGER_QSPIFI_Test_Board = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  _HW_Read,
  _HW_Write,
  NULL,
  NULL,
  NULL,
  NULL,
  _HW_Delay,
  _HW_Lock,
  _HW_Unlock,
  NULL,
  NULL
};

/*************************** End of file ****************************/
