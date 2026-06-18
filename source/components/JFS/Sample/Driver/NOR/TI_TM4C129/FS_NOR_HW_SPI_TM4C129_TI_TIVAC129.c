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

File        : FS_NOR_HW_SPI_X_TM4C129_TI_TIVAC129.c
Purpose     : NOR SPI hardware layer for TI Tiva TM4C129X Development Board.
Literature  :
  [1] Tiva TM4C129XNCZAD Microcontroller DATA SHEET
    (\\fileserver\Techinfo\Company\TI\MCU\TM4C\TM4C129\tm4c129xnczad.pdf)
  [2] Tiva TM4C129X Development Board User's Guide
    (\\fileserver\Techinfo\Company\TI\MCU\TM4C\Evalboard\Tiva_C_series\spmu360.pdf)

Additional information
  The SD card and NOR flash device share the same CS signal. In order to
  be able to work with the SD card, the jumper J7 has to be configured
  according to "2.1.10 EEPROM and SD Card" in [2].
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
#ifndef   FS_NOR_HW_PERIPH_CLOCK_KHZ
  #define FS_NOR_HW_PERIPH_CLOCK_KHZ          120000uL    // Clock of SSI peripheral in kHz
#endif

#ifndef   FS_NOR_HW_SPI_CLOCK_KHZ
  #define FS_NOR_HW_SPI_CLOCK_KHZ             30000uL     // Data transfer clock in kHz (up to 60MHz, HW limitation, Note: Clock speeds > 30MHz do not seem to work).
#endif

#ifndef   FS_NOR_HW_WAIT_TIMEOUT_LOOPS
  #define FS_NOR_HW_WAIT_TIMEOUT_LOOPS        100000uL    // Time to wait for an operation to finish (software loops)
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA                   1           // Enables/disables the data transfer via DMA
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       SSI registers
*/
#define SSI_BASE_ADDR             0x4000B000uL
#define SSICR0                    (*(volatile U32 *)(SSI_BASE_ADDR + 0x000))      // SSI Control 0
#define SSICR1                    (*(volatile U32 *)(SSI_BASE_ADDR + 0x004))      // SSI Control 1
#define SSIDR                     (*(volatile U32 *)(SSI_BASE_ADDR + 0x008))      // SSI Data
#define SSISR                     (*(volatile U32 *)(SSI_BASE_ADDR + 0x00C))      // SSI Status
#define SSICPSR                   (*(volatile U32 *)(SSI_BASE_ADDR + 0x010))      // SSI Clock Prescale
#define SSIIM                     (*(volatile U32 *)(SSI_BASE_ADDR + 0x014))      // SSI Interrupt Mask
#define SSIRIS                    (*(volatile U32 *)(SSI_BASE_ADDR + 0x018))      // SSI Raw Interrupt Status
#define SSIMIS                    (*(volatile U32 *)(SSI_BASE_ADDR + 0x01C))      // SSI Masked Interrupt Status
#define SSIICR                    (*(volatile U32 *)(SSI_BASE_ADDR + 0x020))      // SSI Interrupt Clear
#define SSIDMACTL                 (*(volatile U32 *)(SSI_BASE_ADDR + 0x024))      // SSI DMA Control
#define SSICC                     (*(volatile U32 *)(SSI_BASE_ADDR + 0xFC8))      // SSI Clock Configuration

/*********************************************************************
*
*       System control registers
*/
#define SYS_BASE_ADDR             0x400FE000uL
#define RCGCGPIO                  (*(volatile U32 *)(SYS_BASE_ADDR + 0x608))      // GPIO Run Mode Clock Gating Control
#define RCGCDMA                   (*(volatile U32 *)(SYS_BASE_ADDR + 0x60C))      // DMA Run Mode Clock Gating Control
#define RCGCSSI                   (*(volatile U32 *)(SYS_BASE_ADDR + 0x61C))      // SSI Run Mode Clock Gating Control
#define PRGPIO                    (*(volatile U32 *)(SYS_BASE_ADDR + 0xA08))      // GPIO Run Mode Peripheral Ready
#define PRDMA                     (*(volatile U32 *)(SYS_BASE_ADDR + 0xA0C))      // DMA Run Mode Peripheral Ready
#define PRSSI                     (*(volatile U32 *)(SYS_BASE_ADDR + 0xA1C))      // SSI Run Mode Peripheral Ready

/*********************************************************************
*
*       GPIO Port F
*/
#define PF_BASE_ADDR              0x4005D000uL
#define PF_GPIOAFSEL              (*(volatile U32 *)(PF_BASE_ADDR + 0x420))       // GPIO Alternate Function Select
#define PF_GPIODR2R               (*(volatile U32 *)(PF_BASE_ADDR + 0x500))       // GPIO 2-mA Drive Select
#define PF_GPIODR4R               (*(volatile U32 *)(PF_BASE_ADDR + 0x504))       // GPIO 4-mA Drive Select
#define PF_GPIODR8R               (*(volatile U32 *)(PF_BASE_ADDR + 0x508))       // GPIO 8-mA Drive Select
#define PF_GPIOODR                (*(volatile U32 *)(PF_BASE_ADDR + 0x50C))       // GPIO Open Drain Select
#define PF_GPIOPUR                (*(volatile U32 *)(PF_BASE_ADDR + 0x510))       // GPIO Pull-Up Select
#define PF_GPIOPDR                (*(volatile U32 *)(PF_BASE_ADDR + 0x514))       // GPIO Pull-Down Select
#define PF_GPIODEN                (*(volatile U32 *)(PF_BASE_ADDR + 0x51C))       // GPIO Digital Enable
#define PF_GPIOPCTL               (*(volatile U32 *)(PF_BASE_ADDR + 0x52C))       // GPIO Port Control
#define PF_GPIOPC                 (*(volatile U32 *)(PF_BASE_ADDR + 0xFC4))       // GPIO Peripheral Configuration

/*********************************************************************
*
*       GPIO Port Q
*/
#define PQ_BASE_ADDR              0x40066000uL
#define PQ_GPIODATA               (*(volatile U32 *)(PQ_BASE_ADDR + 0x000))       // GPIO Data
#define PQ_GPIODIR                (*(volatile U32 *)(PQ_BASE_ADDR + 0x400))       // GPIO Direction
#define PQ_GPIOAFSEL              (*(volatile U32 *)(PQ_BASE_ADDR + 0x420))       // GPIO Alternate Function Select
#define PQ_GPIODR2R               (*(volatile U32 *)(PQ_BASE_ADDR + 0x500))       // GPIO 2-mA Drive Select
#define PQ_GPIODR4R               (*(volatile U32 *)(PQ_BASE_ADDR + 0x504))       // GPIO 4-mA Drive Select
#define PQ_GPIODR8R               (*(volatile U32 *)(PQ_BASE_ADDR + 0x508))       // GPIO 8-mA Drive Select
#define PQ_GPIOODR                (*(volatile U32 *)(PQ_BASE_ADDR + 0x50C))       // GPIO Open Drain Select
#define PQ_GPIOPUR                (*(volatile U32 *)(PQ_BASE_ADDR + 0x510))       // GPIO Pull-Up Select
#define PQ_GPIOPDR                (*(volatile U32 *)(PQ_BASE_ADDR + 0x514))       // GPIO Pull-Down Select
#define PQ_GPIODEN                (*(volatile U32 *)(PQ_BASE_ADDR + 0x51C))       // GPIO Digital Enable
#define PQ_GPIOPCTL               (*(volatile U32 *)(PQ_BASE_ADDR + 0x52C))       // GPIO Port Control
#define PQ_GPIOPC                 (*(volatile U32 *)(PQ_BASE_ADDR + 0xFC4))       // GPIO Peripheral Configuration

/*********************************************************************
*
*       DMA controller
*/
#define DMA_BASE_ADDR             0x400FF000uL
#define DMACFG                    (*(volatile U32 *)(DMA_BASE_ADDR + 0x004))      // DMA configuration
#define DMACTLBASE                (*(volatile U32 *)(DMA_BASE_ADDR + 0x008))      // DMA Channel Control Base Pointer
#define DMAUSEBURSTCLR            (*(volatile U32 *)(DMA_BASE_ADDR + 0x01C))      // DMA Channel Useburst Clear
#define DMAREQMASKCLR             (*(volatile U32 *)(DMA_BASE_ADDR + 0x024))      // DMA Channel Request Mask Clear
#define DMAENASET                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x028))      // DMA Channel Enable Set
#define DMAENACLR                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x02C))      // DMA Channel Enable Clear
#define DMAALTCLR                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x034))      // DMA Channel Primary Alternate Clear
#define DMAPRIOCLR                (*(volatile U32 *)(DMA_BASE_ADDR + 0x03C))      // DMA Channel Priority Clear
#define DMACHMAP0                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x510))      // DMA Channel Map Select 0
#define DMACHMAP1                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x514))      // DMA Channel Map Select 1
#define DMACHMAP2                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x518))      // DMA Channel Map Select 2
#define DMACHMAP3                 (*(volatile U32 *)(DMA_BASE_ADDR + 0x51C))      // DMA Channel Map Select 3

/*********************************************************************
*
*       Flash controller
*/
#define FLASH_BASE_ADDR           0x400FD000uL
#define FLASHPP                   (*(volatile U32 *)(FLASH_BASE_ADDR + 0xFC0))    // Flash Peripheral Properties
#define FLASHDMASZ                (*(volatile U32 *)(FLASH_BASE_ADDR + 0xFD0))    // Flash DMA Address Size
#define FLASHDMAST                (*(volatile U32 *)(FLASH_BASE_ADDR + 0xFD4))    // Flash DMA Starting Address

/*********************************************************************
*
*       Pin assignment
*/
#define CS_PIN                    1     // Port Q
#define CLK_PIN                   0     // Port Q
#define DAT0_PIN                  2     // Port Q
#define DAT1_PIN                  0     // Port F
#define DAT2_PIN                  4     // Port F
#define DAT3_PIN                  5     // Port F

/*********************************************************************
*
*       Flags in system registers
*/
#define RCGCSSI_SSI3              3
#define RCGCGPIO_GPIOF            5
#define RCGCGPIO_GPIOQ            14

/*********************************************************************
*
*       SSI related defines
*/
#define CR0_DSS                   0
#define CR0_DSS_8BIT              7uL
#define CR0_SPO                   6
#define CR0_SCR                   8
#define CR0_SPH                   7
#define CR0_SCR_MASK              0xFFuL
#define CR1_SSE                   1
#define CR1_MODE                  6
#define CR1_MODE_BI               1uL
#define CR1_MODE_QUAD             2uL
#define CR1_MODE_ADV              3uL
#define CR1_MODE_MASK             3uL
#define CR1_DIR                   8
#define SR_TNF                    1
#define SR_RNE                    2
#define SR_BSY                    4

/*********************************************************************
*
*       DMA related defines
*/
#define DMA_MAX_TRANSFER_SIZE     1024    // in items
#define DMA_CHANNEL_READ          14
#define DMA_CHANNEL_WRITE         15
#define CHMAP1_SSI3_RX            2uL
#define CHMAP1_SSI3_TX            2uL
#define CHMAP_CHSEL_MASK          0xFuL
#define CHCTL_XFERMODE            0
#define CHCTL_XFERMODE_BASIC      1uL
#define CHCTL_XFERSIZE            4
#define CHCTL_XFERSIZE_MASK       0x3FFuL
#define CHCTL_ARBSIZE             14
#define CHCTL_ARBSIZE_4           2uL
#define CHCTL_SRCINC              26
#define CHCTL_SRCINC_NOINC        3uL
#define CHCTL_DSTINC              30
#define CHCTL_DSTINC_NOINC        3uL
#define DMACTL_RXDMAE             0
#define DMACTL_TXDMAE             1
#define CFG_MASTEREN              0
#define PP_DFA                    28

#if (DMA_CHANNEL_READ < DMA_CHANNEL_WRITE)
  #define NUM_DMA_DESC            (DMA_CHANNEL_WRITE + 1)
#else
  #define NUM_DMA_DESC            (DMA_CHANNEL_READ + 1)
#endif

/*********************************************************************
*
*       Local data types
*
**********************************************************************
*/

/*********************************************************************
*
*       DMA_DESC
*/
typedef struct DMA_DESC {
  volatile U32 SRCENDP;
  volatile U32 DESTENDP;
  volatile U32 CHCTL;
  volatile U32 Reserved;
} DMA_DESC;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
#if FS_NOR_HW_USE_DMA
  //
  // The DMA descriptors have to aligned on a 1KB boundary.
  //
#ifdef __ICCARM__
  #pragma data_alignment=1024
  static DMA_DESC _aDMADesc[NUM_DMA_DESC];
#endif
#ifdef __SES_ARM
  static DMA_DESC _aDMADesc[NUM_DMA_DESC] __attribute__ ((aligned (1024)));
#endif
  static volatile U8 _Dummy;
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _EnableClocks
*/
static void _EnableClocks(void) {
  U32 TimeOut;

  //
  // Provide a clock to SSI module and wait for it to become ready.
  //
  RCGCSSI |= 1uL << RCGCSSI_SSI3;
  TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
  while (1) {
    if (PRSSI & (1uL << RCGCSSI_SSI3)) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
  //
  // Provide a clock to GPIO ports and wait for them to become ready.
  //
  RCGCGPIO |= 0
           | (1uL << RCGCGPIO_GPIOF)
           | (1uL << RCGCGPIO_GPIOQ)
           ;
  TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
  while (1) {
    if (PRGPIO & ((1uL << RCGCGPIO_GPIOF) | (1uL << RCGCGPIO_GPIOQ))) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
#if FS_NOR_HW_USE_DMA
  //
  // Provide a clock to DMA controller and wait for it to become ready.
  //
  RCGCDMA |= 1uL << 0;
  TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
  while (1) {
    if (PRDMA & ((1uL << 0))) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
#endif
}

/*********************************************************************
*
*       _InitPins
*/
static void _InitPins(void) {
  //
  // Give the control of CLK, DAT0-3 signals to SSI module.
  //
  PQ_GPIOAFSEL |= 0
               |  (1uL << CLK_PIN)
               |  (1uL << DAT0_PIN)
               ;
  PF_GPIOAFSEL |= 0
               |  (1uL << DAT1_PIN)
               |  (1uL << DAT2_PIN)
               |  (1uL << DAT3_PIN)
               ;
  PQ_GPIOPCTL  &= ~((0xFuL << (CLK_PIN  << 2)) |
                    (0xFuL << (DAT0_PIN << 2)));
  PQ_GPIOPCTL  |= 0
               |  (0xEuL << (CLK_PIN  << 2))
               |  (0xEuL << (DAT0_PIN << 2))
               ;
  PF_GPIOPCTL  &= ~((0xFuL << (DAT1_PIN << 2)) |
                    (0xFuL << (DAT2_PIN << 2)) |
                    (0xFuL << (DAT3_PIN << 2)));
  PF_GPIOPCTL  |= 0
               |  (0xEuL << (DAT1_PIN << 2))
               |  (0xEuL << (DAT2_PIN << 2))
               |  (0xEuL << (DAT3_PIN << 2))
               ;
  //
  // Set the output drive strength to 2mA as done in the TI sample code.
  //
  PQ_GPIOPC    &= ~((0x3uL << (CLK_PIN  << 1)) |
                    (0x3uL << (DAT0_PIN << 1)));
  PF_GPIOPC    &= ~((0x3uL << (DAT1_PIN << 1)) |
                    (0x3uL << (DAT2_PIN << 1)) |
                    (0x3uL << (DAT3_PIN << 1)));
  PQ_GPIODR2R  |= 0
               |  (1uL << CLK_PIN)
               |  (1uL << DAT0_PIN)
               ;
  PQ_GPIODR4R  &= ~((0x1uL << CLK_PIN) |
                    (0x1uL << DAT0_PIN));
  PQ_GPIODR8R  &= ~((0x1uL << CLK_PIN) |
                    (0x1uL << DAT0_PIN));
  PF_GPIODR2R  |= 0
               |  (1uL << DAT1_PIN)
               |  (1uL << DAT2_PIN)
               |  (1uL << DAT3_PIN)
               ;
  PF_GPIODR4R  &= ~((0x1uL << DAT1_PIN) |
                    (0x1uL << DAT2_PIN) |
                    (0x1uL << DAT3_PIN));
  PF_GPIODR8R  &= ~((0x1uL << DAT1_PIN) |
                    (0x1uL << DAT2_PIN) |
                    (0x1uL << DAT3_PIN));
  //
  // Enable pull-ups on all signal lines.
  //
  PQ_GPIOPUR   |= 0
               |  (1uL << CLK_PIN)
               |  (1uL << DAT0_PIN)
               ;
  PF_GPIOPUR   |= 0
               |  (1uL << DAT1_PIN)
               |  (1uL << DAT2_PIN)
               |  (1uL << DAT3_PIN)
               ;
  PQ_GPIOODR   &= ~((0x1uL << CLK_PIN) |
                    (0x1uL << DAT0_PIN));
  PF_GPIOODR   &= ~((0x1uL << DAT1_PIN) |
                    (0x1uL << DAT2_PIN) |
                    (0x1uL << DAT3_PIN));
  PQ_GPIOPDR   &= ~((0x1uL << CLK_PIN) |
                    (0x1uL << DAT0_PIN));
  PF_GPIOPDR   &= ~((0x1uL << DAT1_PIN) |
                    (0x1uL << DAT2_PIN) |
                    (0x1uL << DAT3_PIN));
  //
  // Enable the pins.
  //
  PQ_GPIODEN   |= 0
               |  (1uL << CLK_PIN)
               |  (1uL << DAT0_PIN)
               ;
  PF_GPIODEN   |= 0
               |  (1uL << DAT1_PIN)
               |  (1uL << DAT2_PIN)
               |  (1uL << DAT3_PIN)
               ;
  //
  // The CS signal is controlled by the driver.
  //
  PQ_GPIODIR   |=   1uL << CS_PIN;
  PQ_GPIOAFSEL &= ~(1uL << CS_PIN);
  PQ_GPIOPUR   &= ~(1uL << CS_PIN);
  PQ_GPIOPDR   &= ~(1uL << CS_PIN);
  PQ_GPIOODR   &= ~(1uL << CS_PIN);
  PQ_GPIODEN   |=   1uL << CS_PIN;
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       _InitDMA
*/
static void _InitDMA(void) {
  DMACFG         |= 1uL << CFG_MASTEREN;        // Enable the DMA controller.
  DMACTLBASE      = (U32)_aDMADesc;             // The descriptor table must be 1024 aligned.
  DMAPRIOCLR      = 0                           // Set the priority to default.
                  | (1uL << DMA_CHANNEL_READ)
                  | (1uL << DMA_CHANNEL_WRITE)
                  ;
  DMAALTCLR       = 0                           // Use the primary control structure in the DMA descriptor table.
                  | (1uL << DMA_CHANNEL_READ)
                  | (1uL << DMA_CHANNEL_WRITE)
                  ;
  DMAUSEBURSTCLR  = 0                           // Respond to single and burst requests.
                  | (1uL << DMA_CHANNEL_READ)
                  | (1uL << DMA_CHANNEL_WRITE)
                  ;
  DMAREQMASKCLR   = 0                           // Allow the recognition of requests.
                  | (1uL << DMA_CHANNEL_READ)
                  | (1uL << DMA_CHANNEL_WRITE)
                  ;
  //
  // Receive requests from SSI.
  //
  DMACHMAP1      &= ~((CHMAP_CHSEL_MASK << ((DMA_CHANNEL_READ - 8) << 2)) |
                      (CHMAP_CHSEL_MASK << ((DMA_CHANNEL_WRITE - 8) << 2)));
  DMACHMAP1      |= (CHMAP1_SSI3_RX << ((DMA_CHANNEL_READ  - 8) << 2))
                 |  (CHMAP1_SSI3_TX << ((DMA_CHANNEL_WRITE - 8) << 2))
                 ;
  //
  // Give DMA access to internal flash memory.
  //
  FLASHPP    |= 1uL << PP_DFA;
  FLASHDMASZ  = 0x3FFFFuL;
  FLASHDMAST  = 0;               // Internal flash memory starts at address 0.
}

/*********************************************************************
*
*       _StartDMAWrite
*/
static void _StartDMAWrite(const U8 * pData, U32 NumBytes) {
  U32 TimeOut;
  U32 CHCTLReg;
  DMA_DESC * pDMADesc;

  //
  // Stop the DMA channel if required.
  //
  if (DMAENASET & (1uL << DMA_CHANNEL_WRITE)) {
    DMAENACLR = 1uL << DMA_CHANNEL_WRITE;
    TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
    while (1) {
      if ((DMAENASET & (1uL << DMA_CHANNEL_WRITE)) == 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  //
  // Configure the data transfer.
  //
  NumBytes = (NumBytes - 1) & CHCTL_XFERSIZE_MASK;
  CHCTLReg = 0
           | (CHCTL_DSTINC_NOINC   << CHCTL_DSTINC)
           | (CHCTL_ARBSIZE_4      << CHCTL_ARBSIZE)
           | (NumBytes             << CHCTL_XFERSIZE)
           | (CHCTL_XFERMODE_BASIC << CHCTL_XFERMODE)
           ;
  //
  // Fill in the DMA descriptor.
  //
  pDMADesc = &_aDMADesc[DMA_CHANNEL_WRITE];
  pDMADesc->SRCENDP  = (U32)(pData + NumBytes);
  pDMADesc->DESTENDP = (U32)&SSIDR;
  pDMADesc->CHCTL    = CHCTLReg;
  //
  // Start the DMA transfer.
  //
  SSIDMACTL  = 1uL << DMACTL_TXDMAE;
  DMAENASET |= 1uL << DMA_CHANNEL_WRITE;
}

/*********************************************************************
*
*       _StartDMARead
*/
static void _StartDMARead(U8 * pData, U32 NumBytes) {
  U32 TimeOut;
  U32 CHCTLReg;
  DMA_DESC * pDMADesc;

  _Dummy = 0xFF;
  FS_USE_PARA(_Dummy);
  //
  // Stop the DMA channels if required.
  //
  if (DMAENASET & ((1uL << DMA_CHANNEL_WRITE) | (1uL << DMA_CHANNEL_READ))) {
    DMAENACLR = 0
              | (1uL << DMA_CHANNEL_WRITE)
              | (1uL << DMA_CHANNEL_READ)
              ;
    TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
    while (1) {
      if ((DMAENASET & ((1uL << DMA_CHANNEL_WRITE) | (1uL << DMA_CHANNEL_READ))) == 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  //
  // Configure the write data transfer.
  //
  NumBytes = (NumBytes - 1) & CHCTL_XFERSIZE_MASK;
  CHCTLReg = 0
           | (CHCTL_DSTINC_NOINC   << CHCTL_DSTINC)
           | (CHCTL_SRCINC_NOINC   << CHCTL_SRCINC)
           | (CHCTL_ARBSIZE_4      << CHCTL_ARBSIZE)
           | (NumBytes             << CHCTL_XFERSIZE)
           | (CHCTL_XFERMODE_BASIC << CHCTL_XFERMODE)
           ;
  //
  // Fill in the DMA descriptor.
  //
  pDMADesc = &_aDMADesc[DMA_CHANNEL_WRITE];
  pDMADesc->SRCENDP  = (U32)&_Dummy;
  pDMADesc->DESTENDP = (U32)&SSIDR;
  pDMADesc->CHCTL    = CHCTLReg;
  //
  // Configure the read data transfer.
  //
  CHCTLReg = 0
           | (CHCTL_SRCINC_NOINC   << CHCTL_SRCINC)
           | (CHCTL_ARBSIZE_4      << CHCTL_ARBSIZE)
           | (NumBytes             << CHCTL_XFERSIZE)
           | (CHCTL_XFERMODE_BASIC << CHCTL_XFERMODE)
           ;
  //
  // Fill in the DMA descriptor.
  //
  pDMADesc = &_aDMADesc[DMA_CHANNEL_READ];
  pDMADesc->SRCENDP  = (U32)&SSIDR;
  pDMADesc->DESTENDP = (U32)(pData + NumBytes);
  pDMADesc->CHCTL    = CHCTLReg;
  //
  // Start the DMA transfer.
  //
  SSIDMACTL  = 0
             | (1uL << DMACTL_RXDMAE)
             | (1uL << DMACTL_TXDMAE)
             ;
  DMAENASET |= 0
            | (1uL << DMA_CHANNEL_READ)
            | (1uL << DMA_CHANNEL_WRITE)
            ;
}

/*********************************************************************
*
*       _WaitForEndOfDMAWrite
*/
static void _WaitForEndOfDMAWrite(void) {
  U32 TimeOut;

  TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
  while (1) {
    if ((DMAENASET & (1uL << DMA_CHANNEL_WRITE)) == 0) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _WaitForEndOfDMARead
*/
static void _WaitForEndOfDMARead(void) {
  U32 TimeOut;

  TimeOut = FS_NOR_HW_WAIT_TIMEOUT_LOOPS;
  while (1) {
    if ((DMAENASET & (1uL << DMA_CHANNEL_READ)) == 0) {
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
}

#endif // FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       _SetMaxSpeed
*/
static U16 _SetMaxSpeed(U16 MaxFreq_kHz) {
  U32 SCRValue;
  U32 CPSValue;
  U32 Factor;

  CPSValue = 0;
  Factor = (FS_NOR_HW_PERIPH_CLOCK_KHZ + (MaxFreq_kHz - 1)) / MaxFreq_kHz;
  do {
    CPSValue += 2;
    SCRValue = ((Factor + (CPSValue - 1)) / CPSValue) - 1;
  } while (SCRValue > 255);
  SSICPSR  = CPSValue;
  SSICR0  &= ~(CR0_SCR_MASK << CR0_SCR);
  SSICR0  |=   SCRValue     << CR0_SCR;
  MaxFreq_kHz = (U16)(FS_NOR_HW_PERIPH_CLOCK_KHZ / (CPSValue * (1 + SCRValue)));
  return MaxFreq_kHz;
}

/*********************************************************************
*
*       _Write
*/
static void _Write(const U8 * pData, int NumBytes) {
#if FS_NOR_HW_USE_DMA
  int NumBytesAtOnce;

  do {
    NumBytesAtOnce = SEGGER_MIN(NumBytes, DMA_MAX_TRANSFER_SIZE);
    _StartDMAWrite(pData, (U32)NumBytesAtOnce);
    _WaitForEndOfDMAWrite();
    NumBytes -= NumBytesAtOnce;
    pData    += NumBytesAtOnce;
  } while (NumBytes);
#else
  do {
    //
    // Wait for room in FIFO.
    //
    while ((SSISR & (1uL << SR_TNF)) == 0) {
      ;
    }
    SSIDR = *pData++;
  } while (--NumBytes);
#endif
  //
  // Wait for the data transfer to finish.
  //
  while (SSISR & (1uL << SR_BSY)) {
    ;
  }
}

/*********************************************************************
*
*       _Read
*/
static void _Read(U8 * pData, int NumBytes) {
#if FS_NOR_HW_USE_DMA
  int NumBytesAtOnce;

  do {
    NumBytesAtOnce = SEGGER_MIN(NumBytes, DMA_MAX_TRANSFER_SIZE);
    _StartDMARead(pData, (U32)NumBytesAtOnce);
    _WaitForEndOfDMARead();
    NumBytes -= NumBytesAtOnce;
    pData    += NumBytesAtOnce;
  } while (NumBytes);
#else
  do {
    //
    // Wait for room in FIFO.
    //
    while ((SSISR & (1uL << SR_TNF)) == 0) {
      ;
    }
    SSIDR = 0xFF;
    //
    // Wait for some data to be received.
    //
    while ((SSISR & (1ul << SR_RNE)) == 0) {
      ;
    }
    *pData++ = (U8)SSIDR;
  } while (--NumBytes);
#endif
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
*    SPI frequency that is set - given in Hz, 0 in case of an error.
*/
static int _HW_Init(U8 Unit) {
  U16          MaxSpeed_kHz;
  volatile U32 Dummy;

  FS_USE_PARA(Unit);
  _EnableClocks();
  _InitPins();
  SSICR1 = 0;
  SSICR0 = 0
         | (CR0_DSS_8BIT << CR0_DSS)
         ;
  SSICC  = 0;
  SSIICR = ~0uL;
  SSIIM  = 0;
  SSIDMACTL = 0;
  MaxSpeed_kHz = _SetMaxSpeed(FS_NOR_HW_SPI_CLOCK_KHZ);
  SSICR1 |= 1uL << CR1_SSE;
  //
  // Empty the receive queue.
  //
  while (1) {
    if ((SSISR & (1uL << SR_RNE)) == 0) {
      break;
    }
    Dummy = SSIDR;
    FS_USE_PARA(Dummy);
  }
#if FS_NOR_HW_USE_DMA
  _InitDMA();
#endif
  return MaxSpeed_kHz;
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
  volatile U32 * pReg;

  FS_USE_PARA(Unit);
  pReg  = (volatile U32 *)((U8 *)&PQ_GPIODATA + ((1uL << CS_PIN) << 2));
  *pReg = ~(1uL << CS_PIN);         // Active low
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
  volatile U32 * pReg;

  FS_USE_PARA(Unit);
  pReg  = (volatile U32 *)((U8 *)&PQ_GPIODATA + ((1uL << CS_PIN) << 2));
  *pReg = 1uL << CS_PIN;            // Active low
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
  //
  // Switch to full-duplex advanced mode.
  //
  SSICR1 &= ~(CR1_MODE_MASK << CR1_MODE);
  SSICR1 |= 0
         | (CR1_MODE_ADV << CR1_MODE)
         | (1uL << CR1_DIR)
         ;
  _Read(pData, NumBytes);
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
  //
  // Switch to transmit advanced mode.
  //
  SSICR1 &= ~((CR1_MODE_MASK << CR1_MODE) |
              (1uL << CR1_DIR));
  SSICR1 |= 0
         | (CR1_MODE_ADV << CR1_MODE)
         ;
  _Write(pData, NumBytes);
}

/*********************************************************************
*
*       _HW_Read_x2
*
*  Function description
*    Reads a specified number of bytes from flash to buffer using 2 data lines.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be read
*/
static void _HW_Read_x2(U8 Unit, U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  //
  // Switch to bi-SSI receive mode.
  //
  SSICR1 &= ~(CR1_MODE_MASK << CR1_MODE);
  SSICR1 |= 0
         | (CR1_MODE_BI << CR1_MODE)
         | (1uL << CR1_DIR)
         ;
  _Read(pData, NumBytes);
}

/*********************************************************************
*
*       _HW_Write_x2
*
*  Function description
*    Writes a specified number of bytes from data buffer to flash using 2 data lines.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*/
static void _HW_Write_x2(U8 Unit, const U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  //
  // Switch to bi-SSI transmit mode.
  //
  SSICR1 &= ~((CR1_MODE_MASK << CR1_MODE) |
              (1uL << CR1_DIR));
  SSICR1 |= 0
         | (CR1_MODE_BI << CR1_MODE)
         ;
  _Write(pData, NumBytes);
}

/*********************************************************************
*
*       _HW_Read_x4
*
*  Function description
*    Reads a specified number of bytes from flash to buffer using 4 data lines.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be read
*/
static void _HW_Read_x4(U8 Unit, U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  //
  // Switch to quad-SSI receive mode.
  //
  SSICR1 &= ~(CR1_MODE_MASK << CR1_MODE);
  SSICR1 |= 0
         | (CR1_MODE_QUAD << CR1_MODE)
         | (1uL << CR1_DIR)
         ;
  _Read(pData, NumBytes);
}

/*********************************************************************
*
*       _HW_Write_x4
*
*  Function description
*    Writes a specified number of bytes from data buffer to flash using 4 data lines.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*/
static void _HW_Write_x4(U8 Unit, const U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  //
  // Switch to quad-SSI send mode.
  //
  SSICR1 &= ~((CR1_MODE_MASK << CR1_MODE) |
              (1uL << CR1_DIR));
  SSICR1 |= 0
         | (CR1_MODE_QUAD << CR1_MODE)
         ;
  _Write(pData, NumBytes);
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_TM4C129_TI_TIVAC129 = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  _HW_Read,
  _HW_Write,
  _HW_Read_x2,
  _HW_Write_x2,
  _HW_Read_x4,
  _HW_Write_x4,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
