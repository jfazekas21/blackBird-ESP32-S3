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

File    : FS_NOR_HW_SPI_GD32F450_GD_GD32450i_EVAL.c
Purpose : Low-level SPI NOR flash driver for the GigaDevice GD32450i-EVAL evaluation board.
Literature:
  [1] Datasheet GD32F450xx ARM Cortex-M4 32-bit MCU
    (\\FILESERVER\Techinfo\Company\GigaDevice\MCUs\GD32F4\GD32F450xx_Datasheet_Rev1.1.pdf)
  [2] User manual GD32F4xx ARM Cortex-M4 32-bit MCU for GD32F405xx, GD32F407xx and GD32F450xx
    (\\FILESERVER\Techinfo\Company\GigaDevice\MCUs\GD32F4\GD32F403_User_Manual_EN_v2.1.pdf)
  [3] User Manual GD32450I-EVAL
    (\\FILESERVER\Techinfo\Company\GigaDevice\MCUs\GD32F4\EvalBoard\GD32450I-EVAL\GD32450I-EVAL_User_Manual-V1.0.pdf)
  [3] Datasheet GD25Q16B
    (\\FILESERVER\Techinfo\Company\GigaDevice\SerialNORFlash\GD25Q16B_Rev1_9.pdf)

Additional information:
  The HOLD, SCK and MOSI signals are shared with Ethernet. The routing
  of the signals is realized via jumpers as follows:

  Signal name     Jumper setting
  ------------------------------
  HOLD            short JP12(2,3)
  SCK             short JP13(2,3)
  MOSI            short JP20(2,3)

  Quad mode is not supported by the current implementation.
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
#ifndef   FS_NOR_HW_SPI_PER_CLK_HZ
  #define FS_NOR_HW_SPI_PER_CLK_HZ          (200000000uL / 4)         // Clock of SPI peripheral
#endif

#ifndef   FS_NOR_HW_SPI_USE_DMA
  #define FS_NOR_HW_SPI_USE_DMA             1                         // Enables/disables the data transfer via DMA.
#endif

#ifndef   FS_NOR_HW_SPI_RESET_DELAY_LOOPS
  #define FS_NOR_HW_SPI_RESET_DELAY_LOOPS   100000                    // Number of software loops to wait for NOR device to reset
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLK_HZ
  #define FS_NOR_HW_SPI_NOR_CLK_HZ          50000000                  // Frequency of the clock supplied to NOR flash device
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Port G registers
*/
#define GPIOG_BASE_ADDR         0x40021800uL
#define GPIOG_MODER             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x00))
#define GPIOG_OSPEEDR           (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x08))
#define GPIOG_PUPDR             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x0C))
#define GPIOG_ODR               (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x14))
#define GPIOG_BSSR              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x18))
#define GPIOG_AFRL              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x20))
#define GPIOG_AFRH              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port I registers
*/
#define GPIOI_BASE_ADDR         0x40022000uL
#define GPIOI_MODER             (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x00))
#define GPIOI_OSPEEDR           (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x08))
#define GPIOI_PUPDR             (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x0C))
#define GPIOI_ODR               (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x14))
#define GPIOI_BSSR              (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x18))
#define GPIOI_AFRL              (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x20))
#define GPIOI_AFRH              (*(volatile U32 *)(GPIOI_BASE_ADDR + 0x24))

/*********************************************************************
*
*       SPI registers
*/
#define SPI5_BASE_ADDR          0x40015400uL
#define SPI_BASE_ADDR           SPI5_BASE_ADDR
#define SPI_CR1                 (*(volatile U32 *)(SPI_BASE_ADDR + 0x00))
#define SPI_CR2                 (*(volatile U32 *)(SPI_BASE_ADDR + 0x04))
#define SPI_SR                  (*(volatile U32 *)(SPI_BASE_ADDR + 0x08))
#define SPI_DR                  (*(volatile U32 *)(SPI_BASE_ADDR + 0x0C))
#define SPI_QCTL                (*(volatile U32 *)(SPI_BASE_ADDR + 0x80))       // This register is available only on the SPI 5 peripheral.

/*********************************************************************
*
*       Reset and clock control
*/
#define RCU_BASE_ADDR           0x40023800uL
#define RCU_AHB1ENR             (*(volatile U32*)(RCU_BASE_ADDR + 0x30))
#define RCU_APB1ENR             (*(volatile U32*)(RCU_BASE_ADDR + 0x40))
#define RCU_APB2ENR             (*(volatile U32*)(RCU_BASE_ADDR + 0x44))

/*********************************************************************
*
*       DMA registers
*/
#define DMA0_BASE_ADDR          0x40026000uL
#define DMA1_BASE_ADDR          0x40026400uL
#define DMA_STREAM_RD           6
#define DMA_STREAM_WR           5
#define DMA_BASE_ADDR           DMA1_BASE_ADDR
#define DMA_LISR                (*(volatile U32 *)(DMA_BASE_ADDR + 0x00))
#define DMA_HISR                (*(volatile U32 *)(DMA_BASE_ADDR + 0x04))
#define DMA_LIFCR               (*(volatile U32 *)(DMA_BASE_ADDR + 0x08))
#define DMA_HIFCR               (*(volatile U32 *)(DMA_BASE_ADDR + 0x0C))
#define DMA_SxCR_RD             (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x10))
#define DMA_SxNDTR_RD           (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x14))
#define DMA_SxPAR_RD            (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x18))
#define DMA_SxM0AR_RD           (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x1C))
#define DMA_SxFCR_RD            (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_RD + 0x24))
#define DMA_SxCR_WR             (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x10))
#define DMA_SxNDTR_WR           (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x14))
#define DMA_SxPAR_WR            (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x18))
#define DMA_SxM0AR_WR           (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x1C))
#define DMA_SxFCR_WR            (*(volatile U32 *)(DMA_BASE_ADDR + 24 * DMA_STREAM_WR + 0x24))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB1ENR_GPIOGEN_BIT     6
#define AHB1ENR_GPIOIEN_BIT     8
#define AHB1ENR_DMA1EN_BIT      22
#define AHB1ENR_DMA0EN_BIT      21
#define APB2ENR_SPI5EN_BIT      21

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_CS_BIT              8   // Port I
#define NOR_CLK_BIT             13  // Port G
#define NOR_MISO_BIT            12  // Port G
#define NOR_MOSI_BIT            14  // Port G
#define NOR_WP_BIT              10  // Port G
#define NOR_HOLD_BIT            11  // Port G

/*********************************************************************
*
*       SPI control register bits
*/
#define CR1_CPHA_BIT            0
#define CR1_CPOL_BIT            1
#define CR1_MSTR_BIT            2
#define CR1_BR_BIT              3
#define CR1_SPE_BIT             6
#define CR1_SSI_BIT             8
#define CR1_SSM_BIT             9
#define CR1_BR_BIT_MAX          0x7
#define CR2_RXDMAEN_BIT         0
#define CR2_TXDMAEN_BIT         1

/*********************************************************************
*
*       SPI status register bits
*/
#define SR_RXNE_BIT             0
#define SR_TXE_BIT              1

/*********************************************************************
*
*       DMA related defines
*/
#define IFCR_CDMEIF_RD_BIT      18
#define IFCR_CTEIF_RD_BIT       19
#define IFCR_CHTIF_RD_BIT       20
#define IFCR_CTCIF_RD_BIT       21
#define IFCR_CDMEIF_WR_BIT      8
#define IFCR_CTEIF_WR_BIT       9
#define IFCR_CHTIF_WR_BIT       10
#define IFCR_CTCIF_WR_BIT       11
#define ISR_TEIF_RD_BIT         19
#define ISR_TCIF_RD_BIT         21
#define ISR_TEIF_WR_BIT         9
#define ISR_TCIF_WR_BIT         11
#define SxCR_EN_BIT             0
#define SxCR_DIR_M2P_BIT        6
#define SxCR_MINC_BIT           10
#define SxCR_PRIO_BIT           16
#define SxCR_PBURST_BIT         21
#define SxCR_MBURST_BIT         23
#define SxCR_CHSEL_BIT          25
#define DMA_CH_SPI              1

/*********************************************************************
*
*       Misc. defines
*/
#define QCLT_IO23_DRV_BIT       2

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U8             _IsInited = 0;
static U32            _Freq_Hz  = 0;
#if FS_NOR_HW_SPI_USE_DMA
  static volatile U32 _Dummy    = 0;
#endif // FS_NOR_HW_SPI_USE_DMA

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
  Freq_Hz    = FS_NOR_HW_SPI_PER_CLK_HZ / 2;
  MaxFreq_Hz = *pFreq_Hz;
  while (1) {
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    Freq_Hz >>= 1;
    ++Div;
    if (Div == CR1_BR_BIT_MAX) {
      break;
    }
  }
  if (Div == 0) {
    Freq_Hz = FS_NOR_HW_SPI_PER_CLK_HZ / 2;
  }
  *pFreq_Hz = Freq_Hz;
  return Div;
}

#if FS_NOR_HW_SPI_USE_DMA

/*********************************************************************
*
*       _StartDMARead
*/
static void _StartDMARead(U8 * pData, U32 NumBytes) {
  //
  // Stop the data transfer and wait for both streams (receive and send) to switch off.
  //
  DMA_SxCR_RD &= ~(1uL << SxCR_EN_BIT);
  while (DMA_SxCR_RD & (1uL << SxCR_EN_BIT)) {
    ;
  }
  DMA_SxCR_WR &= ~(1uL << SxCR_EN_BIT);
  while (DMA_SxCR_WR & (1uL << SxCR_EN_BIT)) {
    ;
  }
  //
  // Configure the receiving stream.
  //
  DMA_SxPAR_RD  = (U32)&SPI_DR;
  DMA_SxM0AR_RD = (U32)pData;
  DMA_SxNDTR_RD = NumBytes;
  DMA_SxCR_RD   = 0
                | (1uL        << SxCR_MINC_BIT)           // Memory increment enable
                | (3uL        << SxCR_PRIO_BIT)           // Set priority to high
                | (1uL        << SxCR_PBURST_BIT)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST_BIT)         // Burst transfer on memory side
                | (DMA_CH_SPI << SxCR_CHSEL_BIT)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_RD  = 0;
  //
  // Configure the sending stream.
  //
  DMA_SxPAR_WR  = (U32)&SPI_DR;
  DMA_SxM0AR_WR = (U32)&_Dummy;
  DMA_SxNDTR_WR = NumBytes;
  DMA_SxCR_WR   = 0
                | (3uL        << SxCR_PRIO_BIT)           // Set priority to high
                | (1uL        << SxCR_PBURST_BIT)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST_BIT)         // Burst transfer on memory side
                | (1uL        << SxCR_DIR_M2P_BIT)        // Memory to peripheral transfer
                | (DMA_CH_SPI << SxCR_CHSEL_BIT)          // Channel connected to SPI peripheral
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
             | (1uL << IFCR_CDMEIF_RD_BIT)
             | (1uL << IFCR_CTEIF_RD_BIT)
             | (1uL << IFCR_CHTIF_RD_BIT)
             | (1uL << IFCR_CTCIF_RD_BIT)
             ;
#if (DMA_STREAM_WR < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_WR_BIT)
             | (1uL << IFCR_CTEIF_WR_BIT)
             | (1uL << IFCR_CHTIF_WR_BIT)
             | (1uL << IFCR_CTCIF_WR_BIT)
             ;
  //
  // Start the data transfer on both streams.
  //
  DMA_SxCR_RD  |= (1uL << SxCR_EN_BIT);
  DMA_SxCR_WR  |= (1uL << SxCR_EN_BIT);
}

/*********************************************************************
*
*       _StartDMAWrite
*/
static void _StartDMAWrite(const U8 * pData, U32 NumBytes) {
  //
  // Stop the data transfer and wait for both streams (receive and send) to switch off.
  //
  DMA_SxCR_RD &= ~(1uL << SxCR_EN_BIT);
  while (DMA_SxCR_RD & (1uL << SxCR_EN_BIT)) {
    ;
  }
  DMA_SxCR_WR &= ~(1uL << SxCR_EN_BIT);
  while (DMA_SxCR_WR & (1uL << SxCR_EN_BIT)) {
    ;
  }
  //
  // Configure the sending stream.
  //
  DMA_SxPAR_WR  = (U32)&SPI_DR;
  DMA_SxM0AR_WR = (U32)pData;
  DMA_SxNDTR_WR = NumBytes;
  DMA_SxCR_WR   = 0
                | (1uL        << SxCR_MINC_BIT)           // Memory increment enable
                | (3uL        << SxCR_PRIO_BIT)           // Set priority to high
                | (1uL        << SxCR_PBURST_BIT)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST_BIT)         // Burst transfer on memory side
                | (1uL        << SxCR_DIR_M2P_BIT)        // Memory to peripheral transfer
                | (DMA_CH_SPI << SxCR_CHSEL_BIT)          // Channel connected to SPI peripheral
                ;
  DMA_SxFCR_WR  = 0;
  //
  // Configure the receiving stream.
  //
  DMA_SxPAR_RD  = (U32)&SPI_DR;
  DMA_SxM0AR_RD = (U32)&_Dummy;
  DMA_SxNDTR_RD = NumBytes;
  DMA_SxCR_RD   = 0
                | (3uL        << SxCR_PRIO_BIT)           // Set priority to high
                | (1uL        << SxCR_PBURST_BIT)         // Burst transfer on peripheral side
                | (1uL        << SxCR_MBURST_BIT)         // Burst transfer on memory side
                | (DMA_CH_SPI << SxCR_CHSEL_BIT)          // Channel connected to SPI peripheral
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
             | (1uL << IFCR_CDMEIF_RD_BIT)
             | (1uL << IFCR_CTEIF_RD_BIT)
             | (1uL << IFCR_CHTIF_RD_BIT)
             | (1uL << IFCR_CTCIF_RD_BIT)
             ;
#if (DMA_STREAM_WR < 4)
  DMA_LIFCR  = 0
#else
  DMA_HIFCR  = 0
#endif
             | (1uL << IFCR_CDMEIF_WR_BIT)
             | (1uL << IFCR_CTEIF_WR_BIT)
             | (1uL << IFCR_CHTIF_WR_BIT)
             | (1uL << IFCR_CTCIF_WR_BIT)
             ;
  //
  // Start the data transfer on both streams.
  //
  DMA_SxCR_RD  |= (1uL << SxCR_EN_BIT);
  DMA_SxCR_WR  |= (1uL << SxCR_EN_BIT);
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
    if (StatusRead & (1uL << ISR_TEIF_RD_BIT)) {
      return 1;       // Error, could not read via DMA.
    }
    if (StatusWrite & (1uL << ISR_TEIF_WR_BIT)) {
      return 1;       // Error, could not write via DMA.
    }
    if ((StatusRead  & (1uL << ISR_TCIF_RD_BIT)) &&
        (StatusWrite & (1uL << ISR_TCIF_WR_BIT))) {
      return 0;       // OK, data transferred via DMA.
    }
  }
}

#endif

#if (FS_NOR_HW_SPI_USE_DMA == 0)

/*********************************************************************
*
*       _TransferByte
*
*  Function description
*    Sends and receives a byte via SPI.
*/
static U8 _TransferByte(U8 Value) {
  while ((SPI_SR & (1uL << SR_TXE_BIT)) == 0) {
    ;
  }
  SPI_DR = (U32)Value;
  while ((SPI_SR & (1uL << SR_RXNE_BIT)) == 0) {
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

  FS_USE_PARA(Unit);
  if (_IsInited == 0) {
    //
    // Enable the clocks of peripherals.
    //
    RCU_AHB1ENR  |= 1uL << AHB1ENR_GPIOGEN_BIT;
    RCU_AHB1ENR  |= 1uL << AHB1ENR_GPIOIEN_BIT;
    RCU_APB2ENR  |= 1uL << APB2ENR_SPI5EN_BIT;
#if FS_NOR_HW_SPI_USE_DMA
    RCU_AHB1ENR  |= 1uL << AHB1ENR_DMA1EN_BIT;
#endif // FS_NOR_HW_SPI_USE_DMA
    //
    // CS is an output signal and is controlled by the driver.
    //
    GPIOI_BSSR     =   1uL   <<  NOR_CS_BIT;         // Deselect NOR flash (active low)
    GPIOI_MODER   &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOI_MODER   |=   1uL   << (NOR_CS_BIT << 1);
    GPIOI_AFRH    &= ~(0xFuL << ((NOR_CS_BIT - 8) << 2));
    GPIOI_OSPEEDR &= ~(3uL   << (NOR_CS_BIT << 1));
    GPIOI_OSPEEDR |=   2uL   << (NOR_CS_BIT << 1);
    GPIOI_PUPDR   |=   2uL   << (NOR_CS_BIT << 1);
    //
    // CLK is an output signal controlled by the SPI peripheral.
    //
    GPIOG_MODER   &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOG_MODER   |=   2uL   << (NOR_CLK_BIT << 1);
    GPIOG_AFRH    &= ~(0xFuL << ((NOR_CLK_BIT - 8) << 2));
    GPIOG_AFRH    |=   0x5uL << ((NOR_CLK_BIT - 8) << 2);
    GPIOG_OSPEEDR &= ~(3uL   << (NOR_CLK_BIT << 1));
    GPIOG_OSPEEDR |=   2uL   << (NOR_CLK_BIT << 1);
    //
    // MOSI is an output signal controlled by the SPI peripheral.
    //
    GPIOG_MODER   &= ~(3uL   <<  (NOR_MOSI_BIT      << 1));
    GPIOG_MODER   |=   2uL   <<  (NOR_MOSI_BIT      << 1);
    GPIOG_AFRH    &= ~(0xFuL << ((NOR_MOSI_BIT - 8) << 2));
    GPIOG_AFRH    |=   0x5uL << ((NOR_MOSI_BIT - 8) << 2);
    GPIOG_OSPEEDR &= ~(3uL   <<  (NOR_MOSI_BIT      << 1));
    GPIOG_OSPEEDR |=   2uL   <<  (NOR_MOSI_BIT      << 1);
    GPIOG_PUPDR   |=   2uL   <<  (NOR_MOSI_BIT      << 1);
    //
    // MISO is an output signal controlled by the SPI peripheral.
    //
    GPIOG_MODER   &= ~(3uL   <<  (NOR_MISO_BIT      << 1));
    GPIOG_MODER   |=   2uL   <<  (NOR_MISO_BIT      << 1);
    GPIOG_AFRH    &= ~(0xFuL << ((NOR_MISO_BIT - 8) << 2));
    GPIOG_AFRH    |=   0x5uL << ((NOR_MISO_BIT - 8) << 2);
    GPIOG_OSPEEDR &= ~(3uL   <<  (NOR_MISO_BIT      << 1));
    GPIOG_OSPEEDR |=   2uL   <<  (NOR_MISO_BIT      << 1);
    GPIOG_PUPDR   |=   1uL   <<  (NOR_MISO_BIT      << 1);
    //
    // WP is an output signal controlled by the SPI peripheral.
    //
    GPIOG_MODER   &= ~(3uL   <<  (NOR_WP_BIT        << 1));
    GPIOG_MODER   |=   2uL   <<  (NOR_WP_BIT        << 1);
    GPIOG_AFRH    &= ~(0xFuL << ((NOR_WP_BIT - 8)   << 2));
    GPIOG_AFRH    |=   0x5uL << ((NOR_WP_BIT - 8)   << 2);
    GPIOG_OSPEEDR &= ~(3uL   <<  (NOR_WP_BIT        << 1));
    GPIOG_OSPEEDR |=   2uL   <<  (NOR_WP_BIT        << 1);
    GPIOG_PUPDR   |=   1uL   <<  (NOR_WP_BIT        << 1);
    //
    // HOLD is an output signal controlled by the SPI peripheral.
    //
    GPIOG_MODER   &= ~(3uL   <<  (NOR_HOLD_BIT      << 1));
    GPIOG_MODER   |=   2uL   <<  (NOR_HOLD_BIT      << 1);
    GPIOG_AFRH    &= ~(0xFuL << ((NOR_HOLD_BIT - 8) << 2));
    GPIOG_AFRH    |=   0x5uL << ((NOR_HOLD_BIT - 8) << 2);
    GPIOG_OSPEEDR &= ~(3uL   <<  (NOR_HOLD_BIT      << 1));
    GPIOG_OSPEEDR |=   2uL   <<  (NOR_HOLD_BIT      << 1);
    GPIOG_PUPDR   |=   1uL   <<  (NOR_HOLD_BIT      << 1);
    //
    // Initialize the SPI controller.
    //
    _Freq_Hz = FS_NOR_HW_SPI_NOR_CLK_HZ;
    Div = (U32)_CalcClockDivider(&_Freq_Hz);
#if FS_NOR_HW_SPI_USE_DMA
    SPI_CR2   = 0
              | (1uL << CR2_RXDMAEN_BIT)
              | (1uL << CR2_TXDMAEN_BIT)
              ;
    FS_USE_PARA(_Dummy);
#endif // FS_NOR_HW_SPI_USE_DMA
    SPI_CR1   = 0
              | (1uL << CR1_MSTR_BIT)
              | (1uL << CR1_SPE_BIT)
              | (1uL << CR1_SSM_BIT)
              | (1uL << CR1_SSI_BIT)
              | (Div << CR1_BR_BIT)
              | (1uL << CR1_CPHA_BIT)
              | (1uL << CR1_CPOL_BIT)
              ;
    SPI_QCTL  = 1uL << QCLT_IO23_DRV_BIT;           // Make sure that the WP and HOLD signals are driven low when the quad mode is disabled. Only available on the SPI5 peripheral.
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
  GPIOI_BSSR |= 1uL << (NOR_CS_BIT + 16);         // Active low
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
  GPIOI_BSSR |= 1uL << NOR_CS_BIT;                // Active low
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
#if FS_NOR_HW_SPI_USE_DMA
  _StartDMARead(pData, (U32)NumBytes);
  _WaitForEndOfDMATransfer();
#else
  do {
    *pData++ = _TransferByte(0xFF);
  } while (--NumBytes);
#endif // FS_NOR_HW_SPI_USE_DMA
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
#if FS_NOR_HW_SPI_USE_DMA
  _StartDMAWrite(pData, (U32)NumBytes);
  _WaitForEndOfDMATransfer();
#else
  do {
    _TransferByte(*pData++);
  } while (--NumBytes);
#endif // FS_NOR_HW_SPI_USE_DMA
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_GD32F450_GD_GD32450i_EVAL = {
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
