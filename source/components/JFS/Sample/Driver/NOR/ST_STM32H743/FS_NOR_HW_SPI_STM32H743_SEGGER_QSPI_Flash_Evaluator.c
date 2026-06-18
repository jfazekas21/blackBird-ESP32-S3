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

File    : FS_NOR_HW_SPI_STM32H743_SEGGER_QSPI_Flash_Evaluator.c
Purpose : Low-level NOR flash driver for STM32H7 using GPIO.
Literature:
  [1] RM0433 Reference manual STM32H7x3 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H7x3_RM0433_Rev3.pdf)
  [2] STM32H753xI Errata sheet STM32H753xI rev Y device limitations
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H753_Errata_Rev2.pdf)
  [3] Data sheet STM32H742xI/G STM32H743xI/G
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\DS12110_STM32H743ZG.pdf)
*/

/*********************************************************************
*
*       #include Section
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
*       Port B registers
*/
#define GPIOB_BASE_ADDR         0x58020400uL
#define GPIOB_MODER             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_OSPEEDR           (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x08))
#define GPIOB_PUPDR             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x0C))
#define GPIOB_ODR               (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x14))
#define GPIOB_BSRR              (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x18))
#define GPIOB_AFRL              (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Port F registers
*/
#define GPIOF_BASE_ADDR         0x58021400uL
#define GPIOF_MODER             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x00))
#define GPIOF_OSPEEDR           (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x08))
#define GPIOF_PUPDR             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x0C))
#define GPIOF_IDR               (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x10))
#define GPIOF_ODR               (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x14))
#define GPIOF_BSRR              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x18))
#define GPIOF_AFRL              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x20))
#define GPIOF_AFRH              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR           0x58024400uL
#define RCC_AHB4ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0xE0))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB4ENR_GPIOBEN         1
#define AHB4ENR_GPIOFEN         5

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_BK1_NCS_BIT         6   // Port B
#define NOR_CLK_BIT             10  // Port F
#define NOR_BK1_D0_BIT          8   // Port F
#define NOR_BK1_D1_BIT          9   // Port F
#define NOR_BK1_D2_BIT          7   // Port F
#define NOR_BK1_D3_BIT          6   // Port F
#define NOR_MP6_BIT             3   // Port F

/*********************************************************************
*
*       GPIO bits and masks
*/
#define OSPEEDR_HIGH            2uL
#define OSPEEDR_MASK            0x3uL
#define MODER_MASK              0x3uL
#define MODER_OUT               1uL
#define MODER_ALT               2uL
#define AFR_MASK                0x3uL

/*********************************************************************
*
*       Data transfer
*/
#define CLR_CS()                GPIOB_BSRR = 1uL << (NOR_BK1_NCS_BIT + 16)
#define SET_CS()                GPIOB_BSRR = 1uL << NOR_BK1_NCS_BIT
#define CLR_CLK()               GPIOF_BSRR = 1uL << (NOR_CLK_BIT + 16)
#define SET_CLK()               GPIOF_BSRR = 1uL << NOR_CLK_BIT
#define CLR_MOSI()              GPIOF_BSRR = 1uL << (NOR_BK1_D0_BIT + 16)
#define SET_MOSI()              GPIOF_BSRR = 1uL << NOR_BK1_D0_BIT
#define GET_MISO()              GPIOF_IDR & (1uL << NOR_BK1_D1_BIT) ? 1 : 0
#define RX_BIT(Bit, Byte)       do { CLR_CLK(); SET_CLK(); if (GET_MISO()) { Byte |= 1 << (Bit); } } while (0)
#define TX_BIT(Bit, Byte)       if (Byte & (1 << Bit)) { SET_MOSI(); } else { CLR_MOSI(); } CLR_CLK(); SET_CLK()

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _RxByte
*
*  Function description
*    Receives a single byte as fast as the CPU can drive the port pins.
*/
static U8 _RxByte(void) {
  U8 Data;

  Data = 0;
  RX_BIT(7, Data);
  RX_BIT(6, Data);
  RX_BIT(5, Data);
  RX_BIT(4, Data);
  RX_BIT(3, Data);
  RX_BIT(2, Data);
  RX_BIT(1, Data);
  RX_BIT(0, Data);
  return Data;
}

/*********************************************************************
*
*       _TxByte
*
*  Function description
*    Sends a single byte as fast as the CPU can drive the port pins.
*/
static void _TxByte(U8 Data) {
  TX_BIT(7, Data);
  TX_BIT(6, Data);
  TX_BIT(5, Data);
  TX_BIT(4, Data);
  TX_BIT(3, Data);
  TX_BIT(2, Data);
  TX_BIT(1, Data);
  TX_BIT(0, Data);
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       _HW_Init
*
*  Function description
*    Initialize the SPI for use with the NOR flash.
*
*  Parameters
*    Unit   Device index
*
*  Return value
*    SPI frequency that is set - given in kHz, 0 in case of an error.
*/
static int _HW_Init(U8 Unit) {
  int SPIFreq_kHz;

  FS_USE_PARA(Unit);
  SPIFreq_kHz = 4000;     // TBD: Measure the actual frequency with the oscilloscope.
  //
  // Enable the clocks of the GPIO peripherals.
  //
  RCC_AHB4ENR  |= 0
               | (1uL << AHB4ENR_GPIOBEN)
               | (1uL << AHB4ENR_GPIOFEN)
               ;
  //
  // NCS of bank 1 is an output signal and is controlled by this HW layer.
  //
  GPIOB_BSRR     =   1uL          << NOR_BK1_NCS_BIT;       // The idle state of the CS signal is HIGH.
  GPIOB_MODER   &= ~(MODER_MASK   << (NOR_BK1_NCS_BIT << 1));
  GPIOB_MODER   |=   MODER_OUT    << (NOR_BK1_NCS_BIT << 1);
  GPIOB_AFRL    &= ~(AFR_MASK     << (NOR_BK1_NCS_BIT << 2));
  GPIOB_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_NCS_BIT << 1));
  GPIOB_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_NCS_BIT << 1);
  //
  // CLK is an output signal controlled by this HW layer.
  //
  GPIOF_BSRR     =   1uL          << NOR_CLK_BIT;           // The idle state of the CLK signal is HIGH.
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_CLK_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_CLK_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_CLK_BIT - 8) << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_CLK_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_CLK_BIT << 1);
  //
  // D0 of bank 1 (MOSI) is an output signal controlled by this HW layer.
  //
  GPIOF_BSRR     =   1uL          << NOR_BK1_D0_BIT;          // The idle state of the MOSI signal is HIGH.
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D0_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_BK1_D0_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_BK1_D0_BIT - 8) << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D0_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D0_BIT << 1);
  //
  // D1 of bank 1 (MISO) is an input signal controlled by this HW layer.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D1_BIT << 1));
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_BK1_D1_BIT - 8) << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D1_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D1_BIT << 1);
  //
  // D2 of bank 1 is not used.
  //
  GPIOF_BSRR     =   1uL          << NOR_BK1_D2_BIT;          // The idle state of the this signal is HIGH.
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D2_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_BK1_D2_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D2_BIT << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D2_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D2_BIT << 1);
  //
  // D3 of bank 1 is not used.
  //
  GPIOF_BSRR     =   1uL          << NOR_BK1_D3_BIT;          // The idle state of the this signal is HIGH.
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D3_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_BK1_D3_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D3_BIT << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D3_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D3_BIT << 1);
  //
  // MP6 is the signal connected to the pin 6 of the NOR flash device
  // which is used by some dual stack devices as the chip select signal
  // for the second stack. We have to disable the second stack
  // by setting the MP6 signal to high in order to prevent that the
  // second stack is accidentally activated.
  //
  GPIOF_BSRR    = 1uL << NOR_MP6_BIT;
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_MP6_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_MP6_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_MP6_BIT << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_MP6_BIT << 1));
  return SPIFreq_kHz;
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
  CLR_CS();
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
  FS_USE_PARA(Unit);
  SET_CS();
}

/*********************************************************************
*
*       _HW_ReadEx
*
*  Function description
*    Reads a specified number of bytes from NOR flash to buffer.
*
*  Parameters
*    Unit       Device index.
*    pData      Pointer to a data buffer.
*    NumBytes   Number of bytes to be read.
*    BusWidth   Number of data lines to be used for the data transfer.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_ReadEx(U8 Unit, U8 * pData, U32 NumBytes, U8 BusWidth) {
  FS_USE_PARA(Unit);
  if (BusWidth > 1) {
    return 1;                   // Error, only data transfers via 1 data line are supported.
  }
  if (NumBytes != 0) {
    SET_MOSI();
    do {
      *pData++ = _RxByte();
    } while (--NumBytes);
  }
  return 0;
}

/*********************************************************************
*
*       _HW_WriteEx
*
*  Function description
*    Writes a specified number of bytes from data buffer to NOR flash.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*    BusWidth   Number of data lines to be used for the data transfer.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_WriteEx(U8 Unit, const U8 * pData, U32 NumBytes, U8 BusWidth) {
  FS_USE_PARA(Unit);
  if (BusWidth > 1) {
    return 1;                   // Error, only data transfers via 1 data line are supported.
  }
  if (NumBytes) {
    do {
      _TxByte(*pData++);
    } while (--NumBytes);
  }
  //
  // Default state of output data line is high
  //
  SET_MOSI();
  return 0;
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_STM32H743_SEGGER_QSPI_Flash_Evaluator = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  _HW_ReadEx,
  _HW_WriteEx
};

/*************************** End of file ****************************/
