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

File      : FS_NOR_HW_SPIFI_STM32L475_ST_DiscoveryKit_IoT_Node.c
Purpose   : Low-level flash driver for STM32L4 QSPI interface.
Literature:
  [1] RM0395 Reference manual STM32L4x5 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\RM0395_STM32L4x5_Rev1_DM00151945.pdf)
  [2] STM32L475xx Errata sheet STM32L475xx device errata
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\ES0302_STM32l475xx.pdf)
  [3] Datasheet STM32L475xx
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\DS10969 _STM32l475xx.pdf)
  [4] UM2153 User manual Discovery kit for IoT node, multi-channel communication with STM32L4
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\Evalboard\STM32L475_Discoverykit_IoT_node\en.DM00347848.pdf)
  [5] Datasheet MX25R6435F
    (\\FILESERVER\Techinfo\Company\Macronix\SPI_NOR_Flash\MX25R6435F_v1.4.pdf)

Notes:
  (1) The Macronix MX25R6435F NOR flash device mounted on the evaluation board works by default
      in ultra low power mode. According to [5] the maximum time required to program a page is
      about 10 ms. For this reason the timeout for the write operation configured via
      FS_NOR_TIMEOUT_PAGE_WRITE has to be set to a value greater than or equal to 10.
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
#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ      36000000uL      // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS          0               // Enables/disables the interrupt-driven operation.
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA         0               // Enables/disables the DMA support.
                                                    // Permitted values:
                                                    //   0 - data transferred via CPU
                                                    //   1 - data transferred via DMA
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "RTOS.h"
  #include "stm32l4xx.h"
  #include "FS_OS.h"
#endif

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       QSPI registers
*/
#define QUADSPI_BASE_ADDR       (0xA0001000u)
#define QUADSPI_CR              (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x00))
#define QUADSPI_DCR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x04))
#define QUADSPI_SR              (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x08))
#define QUADSPI_FCR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x0C))
#define QUADSPI_DLR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x10))
#define QUADSPI_CCR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x14))
#define QUADSPI_AR              (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x18))
#define QUADSPI_ABR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x1C))
#define QUADSPI_DR              (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x20))
#define QUADSPI_DR_BYTE         (*(volatile U8* )(QUADSPI_BASE_ADDR + 0x20))
#define QUADSPI_PSMKR           (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x24))
#define QUADSPI_PSMAR           (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x28))
#define QUADSPI_PIR             (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x2C))
#define QUADSPI_LPTR            (*(volatile U32*)(QUADSPI_BASE_ADDR + 0x30))

/*********************************************************************
*
*       Port E registers
*/
#define GPIOE_BASE_ADDR         (0x48001000u)
#define GPIOE_MODER             (*(volatile U32*)(GPIOE_BASE_ADDR + 0x00))
#define GPIOE_OSPEEDR           (*(volatile U32*)(GPIOE_BASE_ADDR + 0x08))
#define GPIOE_PUPDR             (*(volatile U32*)(GPIOE_BASE_ADDR + 0x0C))
#define GPIOE_ODR               (*(volatile U32*)(GPIOE_BASE_ADDR + 0x14))
#define GPIOE_AFRH              (*(volatile U32*)(GPIOE_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR           (0x40021000u)
#define RCC_CFGR                (*(volatile U32*)(RCC_BASE_ADDR + 0x08))
#define RCC_AHB3RSTR            (*(volatile U32*)(RCC_BASE_ADDR + 0x30))
#define RCC_AHB1ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x48))
#define RCC_AHB2ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x4C))
#define RCC_AHB3ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x50))

/*********************************************************************
*
*       DMA 2 registers
*/
#define DMA2_BASE_ADDR          0x40020400uL
#define DMA2_CHANNEL_INDEX      7
#define DMA2_ISR                (*(volatile U32 *)(DMA2_BASE_ADDR + 0x00))
#define DMA2_IFCR               (*(volatile U32 *)(DMA2_BASE_ADDR + 0x04))
#define DMA2_CCR7               (*(volatile U32 *)(DMA2_BASE_ADDR + 0x08 + 20 * (DMA2_CHANNEL_INDEX - 1)))
#define DMA2_CNDTR7             (*(volatile U32 *)(DMA2_BASE_ADDR + 0x0C + 20 * (DMA2_CHANNEL_INDEX - 1)))
#define DMA2_CPAR7              (*(volatile U32 *)(DMA2_BASE_ADDR + 0x10 + 20 * (DMA2_CHANNEL_INDEX - 1)))
#define DMA2_CMAR7              (*(volatile U32 *)(DMA2_BASE_ADDR + 0x14 + 20 * (DMA2_CHANNEL_INDEX - 1)))
#define DMA2_CSEL               (*(volatile U32 *)(DMA2_BASE_ADDR + 0xA8))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB2ENR_GPIOEEN         4
#define AHB3ENR_QSPIEN          8
#define AHB1ENR_DMA2EN          1

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_NCS_BIT             11    // Port E
#define NOR_CLK_BIT             10    // Port E
#define NOR_D0_BIT              12    // Port E
#define NOR_D1_BIT              13    // Port E
#define NOR_D2_BIT              14    // Port E
#define NOR_D3_BIT              15    // Port E

/*********************************************************************
*
*       GPIO bits and masks
*/
#define OSPEEDR_HIGH            0x2uL
#define OSPEEDR_MASK            0x3uL
#define MODER_ALT               0x2uL
#define MODER_MASK              0x3uL
#define AFR_MASK                0xFuL

/*********************************************************************
*
*       Quad-SPI control register
*/
#define CR_EN_BIT               0
#define CR_ABORT_BIT            1
#define CR_DMAEN_BIT            2
#define CR_FTHRES_BIT           8
#define CR_FTHRES_MASK          0x1Fu
#define CR_TRIE_BIT             16
#define CR_TCIE_BIT             17
#define CR_FTIE_BIT             18
#define CR_SMIE_BIT             19
#define CR_TOIE_BIT             20
#define CR_APMS_BIT             22
#define CR_PRESCALER_BIT        24
#define CR_PRESCALER_MAX        0xFFu

/*********************************************************************
*
*       Quad-SPI device configuration register
*/
#define DCR_CKMODE_BIT          0
#define DCR_FSIZE_BIT           16
#define DCR_FSIZE_MAX           0x1Fu

/*********************************************************************
*
*       Quad-SPI status register
*/
#define SR_TEF_BIT              0
#define SR_TCF_BIT              1
#define SR_FTF_BIT              2
#define SR_SMF_BIT              3
#define SR_TOF_BIT              4
#define SR_BUSY_BIT             5
#define SR_FLEVEL_BIT           8
#define SR_FLEVEL_MASK          0x1Fu

/*********************************************************************
*
*       Quad-SPI communication configuration register
*/
#define CCR_INTRUCTION_BIT      0
#define CCR_IMODE_BIT           8
#define CCR_ADMODE_BIT          10
#define CCR_ADSIZE_BIT          12
#define CCR_ABMODE_BIT          14
#define CCR_ABSIZE_BIT          16
#define CCR_DCYC_BIT            18
#define CCR_DMODE_BIT           24
#define CCR_FMODE_BIT           26
#define CCR_MODE_NONE           0x00uL
#define CCR_MODE_SINGLE         0x01uL
#define CCR_MODE_DUAL           0x02uL
#define CCR_MODE_QUAD           0x03uL
#define CCR_ADSIZE_MASK         0x03uL
#define CCR_ABSIZE_MASK         0x03uL
#define CCR_FMODE_WRITE         0x00uL
#define CCR_FMODE_READ          0x01uL
#define CCR_FMODE_POLL          0x02uL
#define CCR_FMODE_MMAP          0x03uL
#define CCR_FMODE_MASK          0x03uL

/*********************************************************************
*
*       DMA2 related defines
*/
#define IFCR_CGIF7_BIT          24
#define IFCR_CTCIF7_BIT         25
#define IFCR_CHTIF7_BIT         26
#define IFCR_CTEIF7_BIT         27
#define ISR_FEIF7_BIT           22
#define ISR_TEIF7_BIT           25
#define ISR_TCIF7_BIT           27
#define DCCR_EN_BIT             0
#define DCCR_TCIE_BIT           1
#define DCCR_TEIE_BIT           3
#define DCCR_DIR_M2P            1uL
#define DCCR_DIR_BIT            4
#define DCCR_PINC_BIT           6
#define DCCR_MINC_BIT           7
#define DCCR_PSIZE_32BIT        2uL
#define DCCR_PSIZE_BIT          8
#define DCCR_MSIZE_32BIT        2uL
#define DCCR_MSIZE_BIT          10
#define DCCR_PRIO_HIGH          3uL
#define DCCR_PRIO_BIT           12
#define CSELR_CHSEL_QUADSPI     3
#define CSELR_CHSEL_BIT         24
#define CSELR_CHSEL_MASK        0x0FuL

/*********************************************************************
*
*       Misc. defines
*/
#define NUM_BYTES_FIFO          16
#define QUADSPI_PRIO            15
#define DMA2_PRIO               15
#define WAIT_TIMEOUT_MS         1000
#define CYCLES_PER_MS           150           // Blocks the execution for about 1ms on a CPU running at 78 Mhz.
#define WAIT_TIMEOUT_CYCLES     (WAIT_TIMEOUT_MS * CYCLES_PER_MS)

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
extern unsigned int SystemCoreClock;

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  static volatile U32 _StatusQSPI;
#if FS_NOR_HW_USE_DMA
  static volatile U32 _StatusDMA;
#endif
#endif

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CalcClockDivider
*/
static U32 _CalcClockDivider(U32 * pFreq_Hz) {
  U32 Div;
  U32 Freq_Hz;
  U32 MaxFreq_Hz;
  U32 PerClkFreq_Hz;
  U32 AHBPre;
  U32 HPre;

  //
  // Calculate the frequency of the clock supplied to QSPI peripheral.
  //
  HPre          = (RCC_CFGR >> 4) & 0x0FuL;
  AHBPre        = (HPre & 0x08) ? 2uL << (HPre & 0x07) : 1uL;
  PerClkFreq_Hz = (SystemCoreClock / AHBPre);
  //
  // Calculate the divider for the specified QSPI frequency.
  //
  Div        = 0;
  MaxFreq_Hz = *pFreq_Hz;
  for (;;) {
    Freq_Hz = PerClkFreq_Hz / (Div + 1u);
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    ++Div;
    if (Div == CR_PRESCALER_MAX) {
      break;
    }
  }
  *pFreq_Hz = Freq_Hz;
  return Div;
}

/*********************************************************************
*
*       _GetMode
*/
static U32 _GetMode(unsigned BusWidth, U32 NumBytes) {
  U32 Mode;

  Mode = CCR_MODE_NONE;
  if (NumBytes) {
    switch (BusWidth) {
    case 1:
      Mode = CCR_MODE_SINGLE;
      break;
    case 2:
      Mode = CCR_MODE_DUAL;
      break;
    case 4:
      Mode = CCR_MODE_QUAD;
      break;
    default:
      break;
    }
  }
  return Mode;
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
*       _ClearFlags
*/
static void _ClearFlags(void) {
  QUADSPI_FCR = 0
              | (1uL << SR_TEF_BIT)
              | (1uL << SR_TCF_BIT)
              | (1uL << SR_SMF_BIT)
              | (1uL << SR_TOF_BIT)
              ;
  //
  // Wait for the flags to be cleared.
  //
  for (;;) {
    if ((QUADSPI_SR & ((1uL << SR_TEF_BIT) |
                       (1uL << SR_TCF_BIT) |
                       (1uL << SR_SMF_BIT) |
                       (1uL << SR_TOF_BIT))) == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _Cancel
*/
static void _Cancel(void) {
  QUADSPI_CR |= 1uL << CR_ABORT_BIT;
  for (;;) {
    if ((QUADSPI_CR & (1uL << CR_ABORT_BIT)) == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _WaitForTransferComplete
*/
static int _WaitForTransferComplete(void) {
  int r;
  U32 Status;

  r = 0;              // Set to indicate success.
#if FS_NOR_HW_USE_OS
  for (;;) {
    Status = _StatusQSPI;
    if (Status & (1uL << SR_TCF_BIT)) {
      break;
    }
    r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
    if (r != 0) {
      break;
    }
  }
#else
  U32 TimeOut;

  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    Status = QUADSPI_SR;
    if (Status & (1uL << SR_TCF_BIT)) {
      break;
    }
    if (--TimeOut == 0) {
      r = 1;
      break;
    }
  }
#endif
  return r;
}

/*********************************************************************
*
*       _WaitForData
*/
static int _WaitForData(U32 NumBytesExpected) {
  int r;
  U32 Status;
  U32 NumBytesAvail;

  r = 0;              // Set to indicate success.
#if FS_NOR_HW_USE_OS
  for (;;) {
    Status = _StatusQSPI;
    NumBytesAvail = (Status >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
    if ((NumBytesAvail >= 4) || (NumBytesAvail >= NumBytesExpected)) {
      break;
    }
    r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
    if (r != 0) {
      break;
    }
  }
#else
  U32 TimeOut;

  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    Status = QUADSPI_SR;
    NumBytesAvail = (Status >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
    if ((NumBytesAvail >= 4) || (NumBytesAvail >= NumBytesExpected)) {
      break;
    }
    if (--TimeOut == 0) {
      r = 1;
      break;
    }
  }
#endif // FS_NOR_HW_USE_OS
  return r;
}

/*********************************************************************
*
*       _WaitForFreeSpace
*/
static int _WaitForFreeSpace(U32 NumBytesExpected) {
  int r;
  U32 Status;
  U32 NumBytesFree;

  r = 0;              // Set to indicate success.
#if FS_NOR_HW_USE_OS
  for (;;) {
    Status = _StatusQSPI;
    NumBytesFree = (Status >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
    NumBytesFree = NUM_BYTES_FIFO - NumBytesFree;
    if ((NumBytesFree >= 4) || (NumBytesFree >= NumBytesExpected)) {
      break;
    }
    r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
    if (r != 0) {
      break;
    }
  }
#else
  U32 TimeOut;

  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    Status = QUADSPI_SR;
    NumBytesFree = (Status >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
    NumBytesFree = NUM_BYTES_FIFO - NumBytesFree;
    if ((NumBytesFree >= 4) || (NumBytesFree >= NumBytesExpected)) {
      break;
    }
    if (--TimeOut == 0) {
      r = 1;
      break;
    }
  }
#endif // FS_NOR_HW_USE_OS
  return r;
}

/*********************************************************************
*
*       _ReadDataViaCPU
*/
static int _ReadDataViaCPU(U8 * pData, U32 NumBytes) {
  int r;
  U32 DataReg;

  do {
    //
    // Wait for the data to be received.
    //
    r = _WaitForData(NumBytes);
    if (r != 0) {
      break;
    }
    //
    // Read data and store it to destination buffer.
    //
    if (NumBytes < 4) {
      //
      // Read single bytes.
      //
      do {
        *pData++ = QUADSPI_DR_BYTE;
      } while (--NumBytes);
    } else {
      //
      // Read 4 bytes at a time.
      //
      DataReg = QUADSPI_DR;
      *pData++ = (U8)DataReg;
      *pData++ = (U8)(DataReg >> 8);
      *pData++ = (U8)(DataReg >> 16);
      *pData++ = (U8)(DataReg >> 24);
      NumBytes -= 4u;
    }
  } while (NumBytes != 0u);
  return r;
}

/*********************************************************************
*
*       _WriteDataViaCPU
*/
static int _WriteDataViaCPU(const U8 * pData, U32 NumBytes) {
  int r;
  U32 DataReg;

  do {
    //
    // Wait for free space in FIFO.
    //
    r = _WaitForFreeSpace(NumBytes);
    if (r != 0) {
      break;
    }
    //
    // Get the data from source buffer and write it.
    //
    if (NumBytes < 4u) {
      //
      // Write single bytes.
      //
      do {
        QUADSPI_DR_BYTE = *pData++;
      } while (--NumBytes);
    } else {
      //
      // Write 4 bytes at a time if possible.
      //
      DataReg  = (U32)*pData++;
      DataReg |= (U32)*pData++ << 8;
      DataReg |= (U32)*pData++ << 16;
      DataReg |= (U32)*pData++ << 24;
      NumBytes -= 4u;
      QUADSPI_DR = DataReg;
    }
  } while (NumBytes != 0u);
  return r;
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       _StartDMATransfer
*/
static void _StartDMATransfer(U32 * pData, U32 NumBytes, int IsWrite) {
  DMA2_CCR7   &= ~(1uL << DCCR_EN_BIT);                   // Stop the data transfer.
  while ((DMA2_CCR7 & (1uL << DCCR_EN_BIT)) != 0u) {      // Wait for the stream to switch off.
    ;
  }
  DMA2_CPAR7  = (U32)&QUADSPI_DR;
  DMA2_CMAR7  = (U32)pData;
  DMA2_CNDTR7 = NumBytes >> 2;                            // We transfer 4 bytes at a time.
  DMA2_IFCR   = 0                                         // Clear any pending interrupts.
              | (1uL << IFCR_CGIF7_BIT)
              | (1uL << IFCR_CTEIF7_BIT)
              | (1uL << IFCR_CHTIF7_BIT)
              | (1uL << IFCR_CTCIF7_BIT)
              ;
  DMA2_CCR7   = 0
              | (DCCR_PSIZE_32BIT << DCCR_PSIZE_BIT)    // Peripheral bus width
              | (DCCR_MSIZE_32BIT << DCCR_MSIZE_BIT)    // Memory bus width
              | (1uL              << DCCR_MINC_BIT)     // Memory increment enable
              | (DCCR_PRIO_HIGH   << DCCR_PRIO_BIT)     // Set priority to high
              ;
  if (IsWrite != 0) {
    DMA2_CCR7 |= DCCR_DIR_M2P << DCCR_DIR_BIT;
  }
#if FS_NOR_HW_USE_OS
  DMA2_CCR7   |= 0
              | (1uL << DCCR_TEIE_BIT)
              | (1uL << DCCR_TCIE_BIT)
              ;
  _StatusDMA  = 0;
#endif // FS_NOR_HW_USE_OS
  DMA2_CSEL &= ~(CSELR_CHSEL_MASK    << CSELR_CHSEL_BIT);
  DMA2_CSEL |=   CSELR_CHSEL_QUADSPI << CSELR_CHSEL_BIT;
  QUADSPI_CR  |= 0u
              | ((4u - 1u) << CR_FTHRES_BIT)              // Signal the DMA when 4 bytes are available or free.
              | (1uL       << CR_DMAEN_BIT)               // Enable the data transfer via DMA in QSPI controller.
              ;
  DMA2_CCR7   |= 1uL << DCCR_EN_BIT;                      // Start the data transfer
}

/*********************************************************************
*
*       _WaitForEndOfDMATransfer
*/
static int _WaitForEndOfDMATransfer(void) {
  U32 StatusQSPI;
  U32 StatusDMA;
  int r;

  r = 1;                    // Set to indicate an error.
#if FS_NOR_HW_USE_OS
  for (;;) {
    StatusQSPI = _StatusQSPI;
    StatusDMA  = _StatusDMA;
    if ((StatusDMA & (1uL << ISR_TEIF7_BIT)) != 0u) {
      break;                                              // Error, could not transfer data via DMA.
    }
    if ((StatusQSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusQSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << ISR_TCIF7_BIT)) != 0) {
      r = 0;                                              // OK, data transferred.
      break;
    }
    r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
    if (r != 0) {
      break;
    }
  }
#else
  U32 TimeOut;

  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    StatusQSPI = QUADSPI_SR;
    StatusDMA  = DMA2_ISR;
    if ((StatusDMA & (1uL << ISR_TEIF7_BIT)) != 0u) {
      break;                                              // Error, could not transfer data via DMA.
    }
    if ((StatusQSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusQSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << ISR_TCIF7_BIT)) != 0) {
      r = 0;                                              // OK, data transferred.
      break;
    }
    if (--TimeOut == 0) {
      break;                                              // Error, a timeout occurred.
    }
  }
#endif // FS_NOR_HW_USE_OS
  if (r != 0) {
    DMA2_CCR7 = 0;                                        // Stop the data transfer in case of an error.
    while ((DMA2_CCR7 & (1uL << DCCR_EN_BIT)) != 0u) {    // Wait for the stream to switch off
      ;
    }
  }
  QUADSPI_CR &= ~((1uL            << CR_DMAEN_BIT) |      // Disable the data transfer via DMA in QSPI controller and reset the FIFO threshold.
                  (CR_FTHRES_MASK << CR_FTHRES_BIT));
  return r;
}

/*********************************************************************
*
*      _IsDataAligned
*/
static int _IsDataAligned(const void * pData, U32 NumBytes) {
  if ((((U32)pData & 3u) == 0u) && ((NumBytes & 3u) == 0u)) {
    return 1;
  }
  return 0;
}

#endif // FS_NOR_HW_USE_DMA

#if FS_NOR_HW_USE_OS

/*********************************************************************
*
*       QUADSPI_IRQHandler
*/
void QUADSPI_IRQHandler(void);
void QUADSPI_IRQHandler(void) {
  OS_EnterInterrupt();

  _StatusQSPI |= QUADSPI_SR;
  QUADSPI_FCR = 0                    // Prevent other interrupts from occurring.
              | (1uL << SR_SMF_BIT)
              | (1uL << SR_TOF_BIT)
              | (1uL << SR_FTF_BIT)
              | (1uL << SR_TCF_BIT)
              | (1uL << SR_TEF_BIT)
              ;
  FS_X_OS_Signal();
  OS_LeaveInterrupt();
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       DMA2_Channel7_IRQHandler
*/
void DMA2_Channel7_IRQHandler(void);
void DMA2_Channel7_IRQHandler(void) {
  U32 Status;

  OS_EnterNestableInterrupt();
  Status      = DMA2_ISR;
  Status     &= 0               // Make sure that we clear only the flags assigned to the DMA stream we use.
             | (1uL << IFCR_CGIF7_BIT)
             | (1uL << IFCR_CTCIF7_BIT)
             | (1uL << IFCR_CHTIF7_BIT)
             | (1uL << IFCR_CTEIF7_BIT)
             ;
  DMA2_IFCR   = Status;         // Clear pending interrupt flags.
  _StatusDMA |= Status;         // Save the status to a static variable and check it in the task.
  FS_X_OS_Signal();             // Wake up the task.
  OS_LeaveNestableInterrupt();
}

#endif // FS_NOR_HW_USE_DMA

#endif // FS_NOR_HW_USE_OS

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
*
*  Notes
*    (1) We do not enable the timeout for the deactivation of the CS signal
*        because of the silicon limitation 2.6.3 "Memory-mapped read operations
*        may fail when timeout counter is enabled" that is documented in [2]
*/
static int _HW_Init(U8 Unit) {
  U32 Div;
  U32 Freq_Hz;

  FS_USE_PARA(Unit);
  //
  // Enable the clocks of peripherals and reset them.
  //
  RCC_AHB2ENR  |= 0
               | (1uL << AHB2ENR_GPIOEEN)
               ;
  RCC_AHB3ENR  |=  (1uL << AHB3ENR_QSPIEN);
  RCC_AHB3RSTR |=  (1uL << AHB3ENR_QSPIEN);
  RCC_AHB3RSTR &= ~(1uL << AHB3ENR_QSPIEN);
#if FS_NOR_HW_USE_DMA
  RCC_AHB1ENR  |= 0
               | (1uL << AHB1ENR_DMA2EN)
               ;
#endif
  //
  // Wait for the unit to exit reset.
  //
  for (;;) {
    if ((RCC_AHB3RSTR & (1uL << AHB3ENR_QSPIEN)) == 0) {
      break;
    }
  }
  //
  // NCS is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_NCS_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_NCS_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_NCS_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_NCS_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_NCS_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_NCS_BIT      << 1));
  //
  // CLK is an output signal controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_CLK_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_CLK_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_CLK_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_CLK_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_CLK_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_CLK_BIT      << 1));
  //
  // D0 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_D0_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_D0_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_D0_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_D0_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D0_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D0_BIT      << 1));
  //
  // D1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_D1_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_D1_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_D1_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_D1_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D1_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D1_BIT      << 1));
  //
  // D2 is an input/output signal and is controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_D2_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_D2_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_D2_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_D2_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D2_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D2_BIT      << 1));
  //
  // D3 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << ( NOR_D3_BIT      << 1));
  GPIOE_MODER   |=  (MODER_ALT    << ( NOR_D3_BIT      << 1));
  GPIOE_AFRH    &= ~(AFR_MASK     << ((NOR_D3_BIT - 8) << 2));
  GPIOE_AFRH    |=  (0xAuL        << ((NOR_D3_BIT - 8) << 2));
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D3_BIT      << 1));
  GPIOE_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D3_BIT      << 1));
#if FS_NOR_HW_USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(QUADSPI_IRQn);
  NVIC_SetPriority(QUADSPI_IRQn, QUADSPI_PRIO);
#if FS_NOR_HW_USE_DMA
  NVIC_DisableIRQ(DMA2_Channel7_IRQn);
  NVIC_SetPriority(DMA2_Channel7_IRQn, DMA2_PRIO);
#endif // FS_NOR_HW_USE_DMA
#endif
  //
  // Initialize the Quad-SPI controller.
  //
  Freq_Hz = FS_NOR_HW_NOR_CLK_HZ;
  Div = _CalcClockDivider(&Freq_Hz);
  QUADSPI_CR   = 0                                    // Note 1
               | (1uL << CR_EN_BIT)                   // Enable the Quad-SPI unit.
               | (Div << CR_PRESCALER_BIT)
               ;
  QUADSPI_DCR  = 0
               | (1uL           << DCR_CKMODE_BIT)    // CLK signals stays HIGH when the NOR flash is not selected.
               | (DCR_FSIZE_MAX << DCR_FSIZE_BIT)     // We set the NOR flash size to maximum since this value is not known at this stage.
               ;
#if FS_NOR_HW_USE_OS
  //
  // Clear interrupt flags and enable the interrupt.
  //
  QUADSPI_CR  |= 0
              | (1uL << CR_APMS_BIT)
              | (1uL << CR_TRIE_BIT)
              | (1uL << CR_TCIE_BIT)
              | (1uL << CR_SMIE_BIT)
              | (1uL << CR_TOIE_BIT)
              ;
  QUADSPI_FCR  = 0
               | (1uL << SR_SMF_BIT)
               | (1uL << SR_TOF_BIT)
               | (1uL << SR_FTF_BIT)
               | (1uL << SR_TCF_BIT)
               | (1uL << SR_TEF_BIT)
               ;
  _StatusQSPI = 0;
  NVIC_EnableIRQ(QUADSPI_IRQn);
#if FS_NOR_HW_USE_DMA
  NVIC_EnableIRQ(DMA2_Channel7_IRQn);
#endif // FS_NOR_HW_USE_DMA
#endif // FS_NOR_HW_USE_OS
  return (int)Freq_Hz;
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

  //
  // Cancel the memory mode so that the BUSY bit goes to 0
  // and we can write to QUADSPI_CCR register.
  //
  if ((QUADSPI_CCR & (CCR_FMODE_MASK << CCR_FMODE_BIT)) == (CCR_FMODE_MMAP << CCR_FMODE_BIT)) {
    _Cancel();
  }
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
  U32 CfgReg;
  U32 CmdMode;
  U32 AddrMode;
  U32 DataMode;
  U32 NumCyclesDummy;
  U32 AddrSize;

  FS_USE_PARA(Unit);            // This device has only one HW unit.
  AddrSize = 0;
  if (NumBytesAddr) {
     AddrSize = (NumBytesAddr - 1) & CCR_ADSIZE_MASK;
  }
  NumCyclesDummy = _GetNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesDummy);  // The dummy bytes are sent using the data mode.
  CmdMode        = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), sizeof(ReadCmd));
  AddrMode       = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  DataMode       = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), 1);                   // We read at least one byte.
  CfgReg = 0
         | (CCR_FMODE_MMAP << CCR_FMODE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (CmdMode        << CCR_IMODE_BIT)
         | (ReadCmd        << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  _ClearFlags();
  QUADSPI_AR  = 0;
  QUADSPI_CCR = CfgReg;
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
  U32 CfgReg;
  U32 CmdMode;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  CmdMode = _GetMode(BusWidth, sizeof(Cmd));
  CfgReg  = 0
          | (CCR_FMODE_WRITE << CCR_FMODE_BIT)
          | (CmdMode         << CCR_IMODE_BIT)
          | (Cmd             << CCR_INTRUCTION_BIT)
          ;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  QUADSPI_DLR = 0;
  QUADSPI_ABR = 0;
  QUADSPI_CCR = CfgReg;
  //
  // Wait until the command has been completed.
  //
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
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
  U32 AddrReg;
  U32 AltReg;
  U32 CfgReg;
  U32 DataMode;
  U32 AddrMode;
  U32 AltMode;
  U32 CmdMode;
  U32 AltSize;
  U32 AddrSize;
  U32 NumBytes;
  U32 NumBytesAlt;
  U32 NumBytesToRead;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  CmdMode     = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), sizeof(Cmd));
  AddrMode    = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode     = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode    = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
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
    NumBytes = NumBytesAlt;
    do {
      AltReg <<= 8;
      AltReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  AddrSize = 0;
  if (NumBytesAddr) {
    AddrSize = (NumBytesAddr - 1) & CCR_ADSIZE_MASK;
  }
  AltSize = 0;
  if (NumBytesAlt) {
    AltSize = (NumBytesAlt - 1) & CCR_ABSIZE_MASK;
  }
  CfgReg = 0
         | (CCR_FMODE_READ << CCR_FMODE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (CmdMode        << CCR_IMODE_BIT)
         | (Cmd            << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
#if FS_NOR_HW_USE_OS
  _StatusQSPI = 0;
#endif // FS_NOR_HW_USE_OS
  NumBytesToRead = NumBytesData - 1;        // 0 means "read 1 byte".
#if FS_NOR_HW_USE_DMA
  if (NumBytesData != 0u) {
    if (_IsDataAligned(pData, NumBytesData) != 0) {
      _StartDMATransfer((U32 *)pData, NumBytesData, 0);
    }
  }
#endif // FS_NOR_HW_USE_DMA
  if (NumBytesData) {
    QUADSPI_DLR = NumBytesToRead;
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    QUADSPI_AR = AddrReg;
  }
  //
  // Read data from NOR flash.
  //
  if (NumBytesData != 0u) {
#if FS_NOR_HW_USE_DMA
    if (_IsDataAligned(pData, NumBytesData) != 0) {
      (void)_WaitForEndOfDMATransfer();
    } else
#endif // FS_NOR_HW_USE_DMA
    {
      (void)_ReadDataViaCPU(pData, NumBytesData);
    }
  }
  //
  // Wait until the data transfer has been completed.
  //
  (void)_WaitForTransferComplete();
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
  U32 AddrReg;
  U32 AltReg;
  U32 CfgReg;
  U32 DataMode;
  U32 AddrMode;
  U32 AltMode;
  U32 CmdMode;
  U32 AddrSize;
  U32 AltSize;
  U32 NumBytes;
  U32 NumBytesAlt;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  CmdMode     = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), sizeof(Cmd));
  AddrMode    = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode     = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode    = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
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
    NumBytes = NumBytesAlt;
    do {
      AltReg <<= 8;
      AltReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  AddrSize = 0;
  if (NumBytesAddr) {
    AddrSize = NumBytesAddr - 1;
  }
  AltSize = 0;
  if (NumBytesAlt) {
    AltSize = NumBytesAlt - 1;
  }
  CfgReg = 0
         | (CCR_FMODE_WRITE << CCR_FMODE_BIT)
         | (DataMode        << CCR_DMODE_BIT)
         | (AltSize         << CCR_ABSIZE_BIT)
         | (AltMode         << CCR_ABMODE_BIT)
         | (AddrSize        << CCR_ADSIZE_BIT)
         | (AddrMode        << CCR_ADMODE_BIT)
         | (CmdMode         << CCR_IMODE_BIT)
         | (Cmd             << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
#if FS_NOR_HW_USE_OS
  _StatusQSPI = 0;
#endif // FS_NOR_HW_USE_OS
  _ClearFlags();
  if (NumBytesData != 0u) {
    QUADSPI_DLR = NumBytesData - 1;       // 0 means "write 1 byte".
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    QUADSPI_AR = AddrReg;
  }
  //
  // Write data to NOR flash.
  //
  if (NumBytesData) {
#if FS_NOR_HW_USE_DMA
    if (_IsDataAligned(pData, NumBytesData) != 0) {
      _StartDMATransfer((U32 *)pData, NumBytesData, 1);
      (void)_WaitForEndOfDMATransfer();
    } else
#endif // FS_NOR_HW_USE_DMA
    {
      (void)_WriteDataViaCPU(pData, NumBytesData);
    }
  }
  //
  // Wait until the data transfer has been completed.
  //
  (void)_WaitForTransferComplete();
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
  r = -1;                 // Set to indicate that the feature is not supported.
#if FS_NOR_HW_USE_OS
  {
    U32 CfgReg;
    U32 DataMode;
    U32 CmdMode;
    U32 NumBytes;
    U32 DataMask;
    U32 DataMatch;
    U32 Status;

    //
    // Fill local variables.
    //
    DataMask    = 1uL           << BitPos;
    DataMatch   = (U32)BitValue << BitPos;
    Delay     <<= 4;          // 16 clock cycles are required to transfer 2 bytes.
    NumBytes    = 1;          // The response consists of only one byte.
    CmdMode     = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), sizeof(Cmd));
    DataMode    = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytes);
    CfgReg = 0
           | (CCR_FMODE_POLL << CCR_FMODE_BIT)
           | (DataMode       << CCR_DMODE_BIT)
           | (CmdMode        << CCR_IMODE_BIT)
           | (Cmd            << CCR_INTRUCTION_BIT)
           ;
    //
    // Wait until the unit is ready for the new command.
    //
    for (;;) {
      if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
        break;
      }
    }
    //
    // Start the poll operation. The end of operation is signaled by interrupt.
    //
    _ClearFlags();
    _StatusQSPI = 0;
    if (NumBytes) {
      QUADSPI_DLR = NumBytes - 1;             // 0 means "read 1 byte".
    }
    QUADSPI_PSMKR = DataMask;
    QUADSPI_PSMAR = DataMatch;
    QUADSPI_PIR   = Delay;
    QUADSPI_FCR   = 1uL << SR_SMF_BIT;
    QUADSPI_CCR   = CfgReg;
    for (;;) {
      Status = _StatusQSPI;
      if (Status & (1uL << SR_SMF_BIT)) {
        r = 0;                                // Bit set to the requested value.
        break;
      }
      r = FS_X_OS_Wait(TimeOut_ms);
      if (r != 0) {
        //
        // The timeout has expired. Cancel the poll operation.
        //
        _Cancel();
        r = 1;                                // A timeout occurred.
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
*    HW layer function. Requests exclusive access to the SPI bus.
*/
static void _HW_Lock(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Unlock
*
*  Function description
*    HW layer function. Releases the exclusive access of the SPI bus.
*
*  Additional information
*    The configuration of the quad SPI controller has to be saved and
*    later restored in the _HW_Lock() function if the application
*    changes it.
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
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32L475_ST_DiscoveryKit_IoT_Node = {
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
