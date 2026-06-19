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

File:    FS_NOR_HW_SPI_STM32F407_ST_STM32F4DISCOVERY.c
Purpose: Low-level SPI NOR flash driver for ST STM32F407.
Literature:
  [1] RM0090 Reference manual STM32F405/415, STM32F407/417, STM32F427/437 and STM32F429/439 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\ReferenceManual(RM0090)_STM32F40x_41x_42x_43x_Rev12_1605.pdf)
  [2] UM1472 User Manual STM32F4DISCOVERY STM32F4 high-performance discovery board
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\EvalBoard\STM32F4-DISCOVERY\STM32F4DISCOVERY_Board_User_Manual_Rev 2_120101.pdf)
  [3] STM32F405xx STM32F407xx Datasheet
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\Datasheet_STM32F405xx_STM32F407xx_Rev4.pdf)
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
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_HW_PER_CLK_HZ
  #define FS_NOR_HW_PER_CLK_HZ            (168000000uL / 4) // Clock of SPI peripheral
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA               1                 // Enables/disables the data transfer via DMA.
#endif

#ifndef   FS_NOR_HW_RESET_DELAY_LOOPS
  #define FS_NOR_HW_RESET_DELAY_LOOPS     100000            // Number of software loops to wait for NOR device to reset
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ            FS_NOR_HW_PER_CLK_HZ        // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_SPI_UNIT
  #define FS_NOR_HW_SPI_UNIT              3                 // Index of the SPI unit connected to NOR flash (2 or 3)
#endif

#ifndef   FS_NOR_HW_USE_WP_PIN
  #define FS_NOR_HW_USE_WP_PIN            1                 // Enables / disables the used of Write Protect pin of NOR flash
#endif

#ifndef   FS_NOR_HW_USE_RESET_PIN
  #define FS_NOR_HW_USE_RESET_PIN         1                 // Enables / disables the used of Reset pin of NOR flash
#endif

#ifndef   FS_NOR_HW_USE_HOLD_PIN
  #define FS_NOR_HW_USE_HOLD_PIN          1                 // Enables / disables the used of Hold pin of NOR flash
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Port A registers
*/
#define GPIOA_BASE_ADDR     0x40020000uL
#define GPIOA_MODER         (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x00))
#define GPIOA_OSPEEDR       (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x08))
#define GPIOA_PUPDR         (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x0C))
#define GPIOA_BSRR          (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x18))
#define GPIOA_AFRL          (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x20))
#define GPIOA_AFRH          (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port B registers
*/
#define GPIOB_BASE_ADDR     0x40020400uL
#define GPIOB_MODER         (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_OSPEEDR       (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x08))
#define GPIOB_PUPDR         (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x0C))
#define GPIOB_BSRR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x18))
#define GPIOB_AFRL          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x20))
#define GPIOB_AFRH          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port C registers
*/
#define GPIOC_BASE_ADDR     0x40020800uL
#define GPIOC_MODER         (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x00))
#define GPIOC_OSPEEDR       (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x08))
#define GPIOC_PUPDR         (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x0C))
#define GPIOC_BSRR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x18))
#define GPIOC_AFRL          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x20))
#define GPIOC_AFRH          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port D registers
*/
#define GPIOD_BASE_ADDR     0x40020C00uL
#define GPIOD_MODER         (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x00))
#define GPIOD_OSPEEDR       (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x08))
#define GPIOD_PUPDR         (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x0C))
#define GPIOD_BSRR          (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x18))
#define GPIOD_AFRL          (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x20))

/*********************************************************************
*
*       SPI registers
*/
#define SPI2_BASE_ADDR      0x40003800uL
#define SPI3_BASE_ADDR      0x40003C00uL
#if (FS_NOR_HW_SPI_UNIT == 2)
  #define SPI_BASE_ADDR     SPI2_BASE_ADDR
#else
  #define SPI_BASE_ADDR     SPI3_BASE_ADDR
#endif
#define SPI_CR1             (*(volatile U32 *)(SPI_BASE_ADDR + 0x00))
#define SPI_CR2             (*(volatile U32 *)(SPI_BASE_ADDR + 0x04))
#define SPI_SR              (*(volatile U32 *)(SPI_BASE_ADDR + 0x08))
#define SPI_DR              (*(volatile U32 *)(SPI_BASE_ADDR + 0x0C))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR       0x40023800uL
#define RCC_AHB1ENR         (*(volatile U32*)(RCC_BASE_ADDR + 0x30))
#define RCC_APB1ENR         (*(volatile U32*)(RCC_BASE_ADDR + 0x40))
#define RCC_APB2ENR         (*(volatile U32*)(RCC_BASE_ADDR + 0x44))

/*********************************************************************
*
*       DMA registers
*/
#define DMA2_BASE_ADDR      0x40026400uL
#define DMA1_BASE_ADDR      0x40026000uL
#if (FS_NOR_HW_SPI_UNIT == 2)
  #define DMA_STREAM_RD     3
  #define DMA_STREAM_WR     4
#else
  #define DMA_STREAM_RD     0
  #define DMA_STREAM_WR     5
#endif
#define DMA_BASE_ADDR       DMA1_BASE_ADDR
#define DMA_LISR            (*(volatile U32 *)(DMA_BASE_ADDR + 0x00))
#define DMA_HISR            (*(volatile U32 *)(DMA_BASE_ADDR + 0x04))
#define DMA_LIFCR           (*(volatile U32 *)(DMA_BASE_ADDR + 0x08))
#define DMA_HIFCR           (*(volatile U32 *)(DMA_BASE_ADDR + 0x0C))
#define DMA_SxCR_RD         (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x10))
#define DMA_SxNDTR_RD       (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x14))
#define DMA_SxPAR_RD        (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x18))
#define DMA_SxM0AR_RD       (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x1C))
#define DMA_SxFCR_RD        (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x24))
#define DMA_SxCR_WR         (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x10))
#define DMA_SxNDTR_WR       (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x14))
#define DMA_SxPAR_WR        (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x18))
#define DMA_SxM0AR_WR       (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x1C))
#define DMA_SxFCR_WR        (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x24))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB1ENR_GPIOAEN     0
#define AHB1ENR_GPIOBEN     1
#define AHB1ENR_GPIOCEN     2
#define AHB1ENR_GPIODEN     3
#define AHB1ENR_DMA2EN      22
#define AHB1ENR_DMA1EN      21
#define APB1ENR_SPI2EN      14
#define APB1ENR_SPI3EN      15

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#if (FS_NOR_HW_SPI_UNIT == 2)
  #define NOR_CS_BIT        12  // Port B
  #define NOR_CLK_BIT       13  // Port B
  #define NOR_MISO_BIT      14  // Port B
  #define NOR_MOSI_BIT      15  // Port B
#else
  #define NOR_CS_BIT        8   // Port A
  #define NOR_CLK_BIT       10  // Port C
  #define NOR_MISO_BIT      11  // Port C
  #define NOR_MOSI_BIT      12  // Port C
#endif

#define NOR_WP_BIT          11  // Port B
#define NOR_HOLD_BIT        2   // Port D
#define NOR_RESET_BIT       10  // Port A

/*********************************************************************
*
*       SPI control register bits
*/
#define CR1_CPHA            0
#define CR1_CPOL            1
#define CR1_MSTR            2
#define CR1_BR              3
#define CR1_SPE             6
#define CR1_SSI             8
#define CR1_SSM             9
#define CR1_BR_MAX          0x7
#define CR2_RXDMAEN         0
#define CR2_TXDMAEN         1

/*********************************************************************
*
*       SPI status register bits
*/
#define SR_RXNE             0
#define SR_TXE              1

/*********************************************************************
*
*       DMA related defines
*/
#if (FS_NOR_HW_SPI_UNIT == 2)
  #define IFCR_CDMEIF_RD    24
  #define IFCR_CTEIF_RD     25
  #define IFCR_CHTIF_RD     26
  #define IFCR_CTCIF_RD     27
  #define IFCR_CDMEIF_WR    2
  #define IFCR_CTEIF_WR     3
  #define IFCR_CHTIF_WR     4
  #define IFCR_CTCIF_WR     5
  #define ISR_TEIF_RD       25
  #define ISR_TCIF_RD       27
  #define ISR_TEIF_WR       3
  #define ISR_TCIF_WR       5
#else
  #define IFCR_CDMEIF_RD    2
  #define IFCR_CTEIF_RD     3
  #define IFCR_CHTIF_RD     4
  #define IFCR_CTCIF_RD     5
  #define IFCR_CDMEIF_WR    8
  #define IFCR_CTEIF_WR     9
  #define IFCR_CHTIF_WR     10
  #define IFCR_CTCIF_WR     11
  #define ISR_TEIF_RD       3
  #define ISR_TCIF_RD       5
  #define ISR_TEIF_WR       9
  #define ISR_TCIF_WR       11
#endif
#define SxCR_EN             0
#define SxCR_TEIE           2
#define SxCR_TCIE           4
#define SxCR_DIR_M2P        6
#define SxCR_MINC           10
#define SxCR_PSIZE          11
#define SxCR_MSIZE          13
#define SxCR_PRIO           16
#define SxCR_PFCTRL         5
#define SxCR_PBURST         21
#define SxCR_MBURST         23
#define SxCR_CHSEL          25
#define SxFCR_DMDIS         2
#define SxFCR_FTH           0
#define DMA_CH_SPI          0uL

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8             _IsInited = 0;
static U32            _Freq_Hz  = 0;
#if FS_NOR_HW_USE_DMA
  static volatile U32 _Dummy    = 0;
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CalcClockDivider
*/
static U8 _CalcClockDivider(U32 * pFreq_Hz) {
  U8  Div;
  U32 Freq_Hz;
  U32 MaxFreq_Hz;

  Div        = 0;
  Freq_Hz    = FS_NOR_HW_PER_CLK_HZ / 2;
  MaxFreq_Hz = *pFreq_Hz;
  while (1) {
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    Freq_Hz >>= 1;
    ++Div;
    if (Div == CR1_BR_MAX) {
      break;
    }
  }
  if (Div == 0) {
    Freq_Hz = FS_NOR_HW_PER_CLK_HZ / 2;
  }
  *pFreq_Hz = Freq_Hz;
  return Div;
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       _StartDMARead
*/
static void _StartDMARead(U8 * pData, U32 NumBytes) {
  //
  // Stop the data transfer and wait for both streams (receive and send) to switch off.
  //
  DMA_SxCR_RD &= ~(1uL << SxCR_EN);
  while (DMA_SxCR_RD & (1uL << SxCR_EN)) {
    ;
  }
  DMA_SxCR_WR &= ~(1uL << SxCR_EN);
  while (DMA_SxCR_WR & (1uL << SxCR_EN)) {
    ;
  }
  //
  // Configure the receiving stream.
  //
  DMA_SxPAR_RD  = (U32)&SPI_DR;
  DMA_SxM0AR_RD = (U32)pData;
  DMA_SxNDTR_RD = NumBytes;
  DMA_SxCR_RD   = 0
                | (1uL        << SxCR_MINC)           // Memory increment enable
                | (3uL        << SxCR_PRIO)           // Set priority to high
                | (1uL        << SxCR_PBURST)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST)         // Burst transfer on memory side
                | (DMA_CH_SPI << SxCR_CHSEL)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_RD  = 0;
  //
  // Configure the sending stream.
  //
  DMA_SxPAR_WR  = (U32)&SPI_DR;
  DMA_SxM0AR_WR = (U32)&_Dummy;
  DMA_SxNDTR_WR = NumBytes;
  DMA_SxCR_WR   = 0
                | (3uL        << SxCR_PRIO)           // Set priority to high
                | (1uL        << SxCR_PBURST)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST)         // Burst transfer on memory side
                | (1uL        << SxCR_DIR_M2P)        // Memory to peripheral transfer
                | (DMA_CH_SPI << SxCR_CHSEL)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_WR  = 0;
  //
  // Clear any pending interrupts.
  //
#if (DMA_STREAM_RD < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_RD)
             | (1uL << IFCR_CTEIF_RD)
             | (1uL << IFCR_CHTIF_RD)
             | (1uL << IFCR_CTCIF_RD)
             ;
#if (DMA_STREAM_WR < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_WR)
             | (1uL << IFCR_CTEIF_WR)
             | (1uL << IFCR_CHTIF_WR)
             | (1uL << IFCR_CTCIF_WR)
             ;
  //
  // Start the data transfer on both streams.
  //
  DMA_SxCR_RD  |= (1uL << SxCR_EN);
  DMA_SxCR_WR  |= (1uL << SxCR_EN);
}

/*********************************************************************
*
*       _StartDMAWrite
*/
static void _StartDMAWrite(const U8 * pData, U32 NumBytes) {
  //
  // Stop the data transfer and wait for both streams (receive and send) to switch off.
  //
  DMA_SxCR_RD &= ~(1uL << SxCR_EN);
  while (DMA_SxCR_RD & (1uL << SxCR_EN)) {
    ;
  }
  DMA_SxCR_WR &= ~(1uL << SxCR_EN);
  while (DMA_SxCR_WR & (1uL << SxCR_EN)) {
    ;
  }
  //
  // Configure the sending stream.
  //
  DMA_SxPAR_WR  = (U32)&SPI_DR;
  DMA_SxM0AR_WR = (U32)pData;
  DMA_SxNDTR_WR = NumBytes;
  DMA_SxCR_WR   = 0
                | (1uL        << SxCR_MINC)           // Memory increment enable
                | (3uL        << SxCR_PRIO)           // Set priority to high
                | (1uL        << SxCR_PBURST)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST)         // Burst transfer on memory side
                | (1uL        << SxCR_DIR_M2P)        // Memory to peripheral transfer
                | (DMA_CH_SPI << SxCR_CHSEL)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_WR  = 0;
  //
  // Configure the receiving stream.
  //
  DMA_SxPAR_RD  = (U32)&SPI_DR;
  DMA_SxM0AR_RD = (U32)&_Dummy;
  DMA_SxNDTR_RD = NumBytes;
  DMA_SxCR_RD   = 0
                | (3uL        << SxCR_PRIO)           // Set priority to high
                | (1uL        << SxCR_PBURST)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST)         // Burst transfer on memory side
                | (DMA_CH_SPI << SxCR_CHSEL)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_RD  = 0;
  //
  // Clear any pending interrupts.
  //
#if (DMA_STREAM_RD < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_RD)
             | (1uL << IFCR_CTEIF_RD)
             | (1uL << IFCR_CHTIF_RD)
             | (1uL << IFCR_CTCIF_RD)
             ;
#if (DMA_STREAM_WR < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_WR)
             | (1uL << IFCR_CTEIF_WR)
             | (1uL << IFCR_CHTIF_WR)
             | (1uL << IFCR_CTCIF_WR)
             ;
  //
  // Start the data transfer on both streams.
  //
  DMA_SxCR_RD  |= (1uL << SxCR_EN);
  DMA_SxCR_WR  |= (1uL << SxCR_EN);
}

/*********************************************************************
*
*       _WaitForEndOfDMATransfer
*/
static int _WaitForEndOfDMATransfer(void) {
  U32 StatusRead;
  U32 StatusWrite;

  while (1) {
#if (DMA_STREAM_RD < 4)
    StatusRead  = DMA_LISR;
#else
    StatusRead  = DMA_HISR;
#endif
#if (DMA_STREAM_WR < 4)
    StatusWrite = DMA_LISR;
#else
    StatusWrite = DMA_HISR;
#endif
    if (StatusRead & (1uL << ISR_TEIF_RD)) {
      return 1;       // Error, could not read via DMA.
    }
    if (StatusWrite & (1uL << ISR_TEIF_WR)) {
      return 1;       // Error, could not write via DMA.
    }
    if ((StatusRead  & (1uL << ISR_TCIF_RD)) &&
        (StatusWrite & (1uL << ISR_TCIF_WR))) {
      return 0;       // OK, data transferred via DMA.
    }
  }
}

#endif

#if (FS_NOR_HW_USE_DMA == 0)

/*********************************************************************
*
*       _TransferByte
*
*  Function description
*    Sends and receives a byte via SPI.
*/
static U8 _TransferByte(U8 Value) {
  while ((SPI_SR & (1uL << SR_TXE)) == 0) {
    ;
  }
  SPI_DR = (U32)Value;
  while ((SPI_SR & (1uL << SR_RXNE)) == 0) {
    ;
  }
  Value = (U8)SPI_DR;
  return Value;
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
*    Frequency of the SPI clock in kHz.
*/
static int _HW_Init(U8 Unit) {
  volatile U8  Dummy;
  U32          Div;
  volatile U32 NumLoops;

  FS_USE_PARA(Unit);
  if (_IsInited == 0) {
    //
    // Enable the clocks of peripherals.
    //
    RCC_AHB1ENR  |= 1uL << AHB1ENR_GPIOAEN;
    RCC_AHB1ENR  |= 1uL << AHB1ENR_GPIOBEN;
    RCC_AHB1ENR  |= 1uL << AHB1ENR_GPIOCEN;
    RCC_AHB1ENR  |= 1uL << AHB1ENR_GPIODEN;
#if (FS_NOR_HW_SPI_UNIT == 2)
    RCC_APB1ENR  |= 1uL << APB1ENR_SPI2EN;
#else
    RCC_APB1ENR  |= 1uL << APB1ENR_SPI3EN;
#endif
#if FS_NOR_HW_USE_DMA
    RCC_AHB1ENR  |= 1uL << AHB1ENR_DMA1EN;
#endif
#if FS_NOR_HW_USE_HOLD_PIN
    //
    // HOLD is an output signal and is controlled by the driver.
    //
    GPIOD_BSRR     =   1uL   <<  NOR_HOLD_BIT;      // Disable hold mode (active low)
    GPIOD_MODER   &= ~(3uL   << (NOR_HOLD_BIT << 1));
    GPIOD_MODER   |=   1uL   << (NOR_HOLD_BIT << 1);
    GPIOD_AFRL    &= ~(0xFuL << (NOR_HOLD_BIT << 2));
    GPIOD_OSPEEDR &= ~(3uL   << (NOR_HOLD_BIT << 1));
#endif
#if FS_NOR_HW_USE_WP_PIN
    //
    // WP is an output signal and is controlled by the driver.
    //
    GPIOB_BSRR     =   1uL   <<  NOR_WP_BIT;          // Disable the write protect mode (active low)
    GPIOB_MODER   &= ~(3uL   << (NOR_WP_BIT << 1));
    GPIOB_MODER   |=   1uL   << (NOR_WP_BIT << 1);
    GPIOB_AFRH    &= ~(0xFuL << ((NOR_WP_BIT - 8) << 2));
    GPIOB_OSPEEDR &= ~(3uL   << (NOR_WP_BIT << 1));
#endif
#if FS_NOR_HW_USE_RESET_PIN
    //
    // RESET is an output signal and is controlled by the driver.
    //
    GPIOA_BSRR     =   1uL   <<  NOR_RESET_BIT;       // Disable reset (active low)
    GPIOA_MODER   &= ~(3uL   << (NOR_RESET_BIT << 1));
    GPIOA_MODER   |=   1uL   << (NOR_RESET_BIT << 1);
    GPIOA_AFRH    &= ~(0xFuL << ((NOR_RESET_BIT - 8) << 2));
    GPIOA_OSPEEDR &= ~(3uL   << (NOR_RESET_BIT << 1));
#endif
#if (FS_NOR_HW_SPI_UNIT == 2)
    //
    // CS is an output signal and is controlled by the driver.
    //
    GPIOB_BSRR     =   1uL   <<  NOR_CS_BIT;         // Deselect NOR flash (active low)
    GPIOB_MODER   &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOB_MODER   |=   1uL   << (NOR_CS_BIT << 1);
    GPIOB_AFRH    &= ~(0xFuL << ((NOR_CS_BIT - 8) << 2));
    GPIOB_OSPEEDR &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOB_OSPEEDR |=   2uL   << (NOR_CS_BIT << 1);
    GPIOB_PUPDR   |=   2uL   << (NOR_CS_BIT << 1);
    //
    // CLK is an output signal controlled by the SPI peripheral.
    //
    GPIOB_MODER   &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOB_MODER   |=   2uL   << (NOR_CLK_BIT << 1);
    GPIOB_AFRH    &= ~(0xFuL << ((NOR_CLK_BIT - 8) << 2));
    GPIOB_AFRH    |=   0x5uL << ((NOR_CLK_BIT - 8) << 2);
    GPIOB_OSPEEDR &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOB_OSPEEDR |=   2uL   << (NOR_CLK_BIT << 1);
    //
    // MOSI is an output signal controlled by the SPI peripheral.
    //
    GPIOB_MODER   &= ~(3uL   << (NOR_MOSI_BIT << 1));
    GPIOB_MODER   |=   2uL   << (NOR_MOSI_BIT << 1);
    GPIOB_AFRH    &= ~(0xFuL << ((NOR_MOSI_BIT - 8) << 2));
    GPIOB_AFRH    |=   0x5uL << ((NOR_MOSI_BIT - 8) << 2);
    GPIOB_OSPEEDR &= ~(3uL   << (NOR_MOSI_BIT << 1));
    GPIOB_OSPEEDR |=   2uL   << (NOR_MOSI_BIT << 1);
    GPIOB_PUPDR   |=   2uL   << (NOR_MOSI_BIT << 1);
    //
    // MISO is an output signal controlled by the SPI peripheral.
    //
    GPIOB_MODER   &= ~(3uL   << (NOR_MISO_BIT << 1));
    GPIOB_MODER   |=   2uL   << (NOR_MISO_BIT << 1);
    GPIOB_AFRH    &= ~(0xFuL << ((NOR_MISO_BIT - 8) << 2));
    GPIOB_AFRH    |=   0x5uL << ((NOR_MISO_BIT - 8) << 2);
    GPIOB_OSPEEDR &= ~(3uL   << (NOR_MISO_BIT << 1));
    GPIOB_OSPEEDR |=   2uL   << (NOR_MISO_BIT << 1);
    GPIOB_PUPDR   |=   1uL   << (NOR_MISO_BIT << 1);
#else
    //
    // CS is an output signal and is controlled by the driver.
    //
    GPIOA_BSRR     =   1uL   <<  NOR_CS_BIT;         // Deselect NOR flash (active low)
    GPIOA_MODER   &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOA_MODER   |=   1uL   << (NOR_CS_BIT << 1);
    GPIOA_AFRH    &= ~(0xFuL << ((NOR_CS_BIT - 8) << 2));
    GPIOA_OSPEEDR &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOA_OSPEEDR |=   2uL   << (NOR_CS_BIT << 1);
    GPIOA_PUPDR   |=   2uL   << (NOR_CS_BIT << 1);
    //
    // CLK is an output signal controlled by the SPI peripheral.
    //
    GPIOC_MODER   &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOC_MODER   |=   2uL   << (NOR_CLK_BIT << 1);
    GPIOC_AFRH    &= ~(0xFuL << ((NOR_CLK_BIT - 8) << 2));
    GPIOC_AFRH    |=   0x6uL << ((NOR_CLK_BIT - 8) << 2);
    GPIOC_OSPEEDR &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOC_OSPEEDR |=   2uL   << (NOR_CLK_BIT << 1);
    //
    // MOSI is an output signal controlled by the SPI peripheral.
    //
    GPIOC_MODER   &= ~(3uL   << (NOR_MOSI_BIT << 1));
    GPIOC_MODER   |=   2uL   << (NOR_MOSI_BIT << 1);
    GPIOC_AFRH    &= ~(0xFuL << ((NOR_MOSI_BIT - 8) << 2));
    GPIOC_AFRH    |=   0x6uL << ((NOR_MOSI_BIT - 8) << 2);
    GPIOC_OSPEEDR &= ~(3uL   << (NOR_MOSI_BIT << 1));
    GPIOC_OSPEEDR |=   2uL   << (NOR_MOSI_BIT << 1);
    GPIOC_PUPDR   |=   2uL   << (NOR_MOSI_BIT << 1);
    //
    // MISO is an output signal controlled by the SPI peripheral.
    //
    GPIOC_MODER   &= ~(3uL   << (NOR_MISO_BIT << 1));
    GPIOC_MODER   |=   2uL   << (NOR_MISO_BIT << 1);
    GPIOC_AFRH    &= ~(0xFuL << ((NOR_MISO_BIT - 8) << 2));
    GPIOC_AFRH    |=   0x6uL << ((NOR_MISO_BIT - 8) << 2);
    GPIOC_OSPEEDR &= ~(3uL   << (NOR_MISO_BIT << 1));
    GPIOC_OSPEEDR |=   2uL   << (NOR_MISO_BIT << 1);
    GPIOC_PUPDR   |=   1uL   << (NOR_MISO_BIT << 1);
#endif
#if FS_NOR_HW_USE_RESET_PIN
    //
    // Reset the NOR device.
    //
    NumLoops = FS_NOR_HW_RESET_DELAY_LOOPS;                     // Wait for the device to finish the last write operation.
    do {
      ;
    } while (--NumLoops);
    GPIOA_BSRR = 1uL << (NOR_RESET_BIT + 16);                   // Enter reset (active low).
    NumLoops = FS_NOR_HW_RESET_DELAY_LOOPS;
    do {
      ;
    } while (--NumLoops);
    GPIOA_BSRR = 1uL << NOR_RESET_BIT;                          // Leave reset (active low)
    NumLoops = FS_NOR_HW_RESET_DELAY_LOOPS;
    do {
      ;
    } while (--NumLoops);
#endif
    //
    // Initialize the SPI controller.
    //
    _Freq_Hz = FS_NOR_HW_NOR_CLK_HZ;
    Div = (U32)_CalcClockDivider(&_Freq_Hz);
#if FS_NOR_HW_USE_DMA
    SPI_CR2   = 0
              | (1uL << CR2_RXDMAEN)
              | (1uL << CR2_TXDMAEN)
              ;
    FS_USE_PARA(_Dummy);
#endif
    SPI_CR1   = 0
              | (1uL << CR1_MSTR)
              | (1uL << CR1_SPE)
              | (1uL << CR1_SSM)
              | (1uL << CR1_SSI)
              | (Div << CR1_BR)
              | (1uL << CR1_CPHA)
              | (1uL << CR1_CPOL)
              ;
    Dummy     = (U8)SPI_DR;                         // Empty the data register.
    FS_USE_PARA(Dummy);
    _IsInited = 1;
  }
  return (int)_Freq_Hz / 1000;
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
#if (FS_NOR_HW_SPI_UNIT == 2)
  GPIOB_BSRR |= 1uL << (NOR_CS_BIT + 16);         // Active low
#else
  GPIOA_BSRR |= 1uL << (NOR_CS_BIT + 16);         // Active low
#endif
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
#if (FS_NOR_HW_SPI_UNIT == 2)
  GPIOB_BSRR |= 1uL << NOR_CS_BIT;                // Active low
#else
  GPIOA_BSRR |= 1uL << NOR_CS_BIT;                // Active low
#endif
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
  FS_USE_PARA(Unit);
#if FS_NOR_HW_USE_DMA
  _StartDMARead(pData, (U32)NumBytes);
  _WaitForEndOfDMATransfer();
#else
  do {
    *pData++ = _TransferByte(0xFF);
  } while (--NumBytes);
#endif
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
  FS_USE_PARA(Unit);
#if FS_NOR_HW_USE_DMA
  _StartDMAWrite(pData, (U32)NumBytes);
  _WaitForEndOfDMATransfer();
#else
  do {
    _TransferByte(*pData++);
  } while (--NumBytes);
#endif
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_STM32F407_ST_STM32F4DISCOVERY = {
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
