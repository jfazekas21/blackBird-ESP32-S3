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

File    : FS_NOR_HW_SPI_STM32F103_IAR_STM32_SK.c
Purpose : NOR SPI hardware layer for ST STM32F103
Literature:
  [1] STM32-SK Schematics
    (\\fileserver\Techinfo\Company\IAR\Tech\KS_Boards\STM32-SK_revB-schematic.pdf)
  [2] RM0008 Reference manual STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx advanced ARM-based 32-bit MCUs
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32F1\STM32F10_RM.pdf)

Additional information:
  The SPI NOR flash is connected via SD card adapter to SPI port 2.
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdio.h>
#include "FS.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_HW_SPI_PERIPH_CLOCK_KHZ
  #define FS_NOR_HW_SPI_PERIPH_CLOCK_KHZ    36000uL   // Clock of SPI peripheral in kHz
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLOCK_KHZ
  #define FS_NOR_HW_SPI_NOR_CLOCK_KHZ       25000u    // SPI communication speed
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Reset and clock control registers
*/
#define RCC_BASE_ADDR       0x40021000uL
#define RCC_CFGR            (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x04))
#define RCC_APB2RSTR        (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x0C))
#define RCC_APB1RSTR        (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x10))
#define RCC_AHBENR          (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x14))
#define RCC_APB2ENR         (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x18))
#define RCC_APB1ENR         (*(volatile unsigned int*)(RCC_BASE_ADDR + 0x1C))

/*********************************************************************
*
*       GPIOB registers
*/
#define GPIOB_BASE_ADDR     0x40010C00uL
#define GPIOB_CRL           (*(volatile unsigned *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_CRH           (*(volatile unsigned *)(GPIOB_BASE_ADDR + 0x04))
#define GPIOB_ODR           (*(volatile unsigned *)(GPIOB_BASE_ADDR + 0x0C))
#define GPIOB_BSSR          (*(volatile unsigned *)(GPIOB_BASE_ADDR + 0x10))

/*********************************************************************
*
*       GPIOC registers
*/
#define GPIOC_BASE_ADDR     0x40011000uL
#define GPIOC_CRL           (*(volatile unsigned *)(GPIOC_BASE_ADDR + 0x00))
#define GPIOC_IDR           (*(volatile unsigned *)(GPIOC_BASE_ADDR + 0x08))

/*********************************************************************
*
*       SPI2 registers
*/
#define SPI2_BASE_ADDR      0x40003800uL
#define SPI2_CR1            (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x00))
#define SPI2_CR2            (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x04))
#define SPI2_SR             (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x08))
#define SPI2_DR             (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x0C))
#define SPI2_CRCPR          (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x10))
#define SPI2_RXCRCR         (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x14))
#define SPI2_TXCRCR         (*(volatile unsigned *)(SPI2_BASE_ADDR + 0x18))

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8 _IsInited;

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SetMaxSpeed
*/
static U16 _SetMaxSpeed(U16 MaxFreq) {
  U32      Div;
  unsigned DivVal;

  Div    = 2;
  DivVal = 0;
  while ((MaxFreq * Div) < FS_NOR_HW_SPI_PERIPH_CLOCK_KHZ) {
    Div <<= 1;
    if (++DivVal == 7) {
      break;
    }
  }
  //
  // Disable the SPI initially before setting the new divider
  //
  SPI2_CR1  &=  ~(1 << 6);
  //
  // Set the divider
  //
  SPI2_CR1  &=  ~(7 << 3);
  SPI2_CR1  |=  (DivVal << 3);
  //
  // Enable SPI
  //
  SPI2_CR1  |=  (1 << 6);
  return (U16)(FS_NOR_HW_SPI_PERIPH_CLOCK_KHZ / Div);    // We are not faster than this.
}

/*********************************************************************
*
*       _ReadWriteSPI
*
*/
static U8 _ReadWriteSPI(U8 Data) {
  //
  // Send data.
  //
  SPI2_DR = Data;
  //
  // Wait until all bits are shifted.
  //
  while (0 == (SPI2_SR & (1 << 0))) {
    ;
  }
  //
  // Read data
  //
  return (U8)SPI2_DR;
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
*    Unit   Device index.
*
*  Return value
*    SPI frequency that is set - given in kHz.
*/
static int _HW_Init(U8 Unit) {
  int Freq_kHz;

  FS_USE_PARA(Unit);
  if (_IsInited == 0) {
    _IsInited = 1;
    RCC_APB2RSTR &= ~(  (1 << 3)      // Remove RESET from PortB
                      | (1 << 4))     // Remove RESET from PortC
                 ;
    RCC_APB2ENR  |= (1 << 3)          // Enable clock for PortB
                 |  (1 << 4)          // Enable clock for PortC
                 ;
    RCC_APB1RSTR &= ~(1 << 14)        // Remove RESET from SPI2
                  ;
    RCC_APB1ENR  |=  (1 << 14)        // Enable clock for SPI2
                  ;
    //
    // Set all port pins to reset state.
    //
    GPIOB_CRH &= ~(  (3uL << 16)
                   | (3uL << 18)
                   | (3uL << 20)
                   | (3uL << 22)
                   | (3uL << 24)
                   | (3uL << 26)
                   | (3uL << 28)
                   | (3uL << 30))
                   ;
    GPIOB_CRH |= (3uL << 16)          // PortB12 -> GPIO output, using PP (used for CS)
              |  (3uL << 20)          // PortB13 -> Output mode, max 50MHz
              |  (2uL << 22)          // PortB13 -> SPI_CLK
              |  (3uL << 24)          // PortB14 -> Output mode, max 50MHz
              |  (2uL << 26)          // PortB14 -> SPI_MOSI
              |  (3uL << 28)          // PortB15 -> Output mode, max 50MHz
              |  (2uL << 30)          // PortB15 -> SPI_MISO
              ;

    //
    // Set all port pins to reset state.
    //
    GPIOC_CRL &= ~(  (3uL << 24)
                   | (3uL << 26)
                   | (3uL << 28)
                   | (3uL << 30))
                   ;
    GPIOC_CRL |= (1 << 26)            // Port C6 - floating input mode used as WP
              |  (1 << 30)            // Port C7 - floating input mode used as CD
              ;
    //
    // Initialize SPI
    //
    SPI2_CR1 = (0 << 0)               // Second clock transition is the first data capture edge
             | (0 << 1)               // Clock polarity is high when idle
             | (1 << 2)               // SPI is master
             | (7 << 3)               // Clock = pclk (72MHz) / 256
             | (1 << 8)
             | (1 << 9)
             ;
    SPI2_CR2 = 0;                     // Neither interrupts nor DMA is used
    SPI2_CR1 |= (1 << 6)              // Enable SPI
               ;
  }
  Freq_kHz = (int)_SetMaxSpeed(FS_NOR_HW_SPI_NOR_CLOCK_KHZ);
  return Freq_kHz;
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Sets the card slot active using the chip select (CS) line.
*
*  Parameters
*    Unit     Device index.
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  GPIOB_BSSR |= 1 << (12 + 16);
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Clears the card slot inactive using the chip select (CS) line.
*
*  Parameters
*    Unit   Device index.
*/
static void _HW_DisableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  GPIOB_BSSR |= 1 << 12;
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Reads a specified number of bytes from flash to buffer.
*
*  Parameters
*    Unit       Device index.
*    pData      Pointer to a data buffer.
*    NumBytes   Number of bytes.
*/
static void _HW_Read(U8 Unit, U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  do {
    *pData++ = _ReadWriteSPI(0xff);
  } while (--NumBytes);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Writes a specified number of bytes from data buffer to serial NOR flash.
*
*  Parameters
*    Unit       Device index.
*    pData      Pointer to a data buffer.
*    NumBytes   Number of bytes.
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  do {
    _ReadWriteSPI(*pData++);
  } while (--NumBytes);
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_STM32F103_IAR_STM32_SK = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  _HW_Read,
  _HW_Write,
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
