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

File      : FS_NOR_HW_SPIFI_STM32F413_ST_32F413HDISCOVERY.c
Purpose   : Low-level flash driver for STM32F4 QSPI interface.
Literature:
  [1] RM0430 Reference manual STM32F413/423 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\ReferenceManual_STM32F413.pdf)
  [2] Datasheet STM32F413xG STM32F413xH
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\Datasheet_STM32F413xH_STM32F413xG.pdf)
  [3] UM2135 User manual Discovery kit with STM32F413ZH MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F4\EvalBoard\STM32F413H-DISCO\UserManual_STM32F413ZH.pdf)
  [4] STM32F413xG/xH STM32F423xH Errata sheet
    (\\FILESERVER\Techinfo_rw\Company\ST\MCU\STM32\STM32F4\ErrataSheet_STM32F413xG_xH_STM32F423xH.pdf)
  [5] AN4031 Application note Using the STM32F2, STM32F4 and STM32F7 Series DMA controller
    (\\FILESERVER\Techinfo_rw\Company\ST\MCU\STM32\STM32F4\dm00046011-using-the-stm32f2-stm32f4-and-stm32f7-series-dma-controller-stmicroelectronics.pdf)

Additional information:
  FS_NOR_HW_USE_DMA_MEM_PORT can be used to work around the silicon
  limitation "2.2.7  In some specific cases, DMA2 data corruption occurs
  when managing AHB and APB2 peripherals in a concurrent way" documented
  in [4] by setting the value of this define to 1.
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
#ifndef   FS_NOR_HW_PER_CLK_HZ
  #define FS_NOR_HW_PER_CLK_HZ          100000000   // Clock of Quad-SPI unit
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ          50000000    // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS              0           // Enables/disables the interrupt-driven operation.
                                                    // Permitted values:
                                                    //   0 - polling via CPU
                                                    //   1 - event-driven using embOS
                                                    //   2 - event-driven using other RTOS
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA             0           // Enables/disables the DMA support.
                                                    // Permitted values:
                                                    //   0 - data transferred via CPU
                                                    //   1 - data transferred via DMA
#endif

#ifndef   FS_NOR_HW_USE_DMA_MEM_PORT
  #define FS_NOR_HW_USE_DMA_MEM_PORT    0           // Configures the DMA port that has to be used for accessing the Quad-SPI unit.
                                                    //   0 - AHB peripheral port
                                                    //   1 - AHB memory port
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "stm32f4xx.h"
  #include "FS_OS.h"
#if (FS_NOR_HW_USE_OS == 1)
  #include "RTOS.h"
#endif // FS_NOR_HW_USE_OS == 1
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
#define QUADSPI_BASE_ADDR       0xA0001000uL
#define QUADSPI_CR              (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x00))
#define QUADSPI_DCR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x04))
#define QUADSPI_SR              (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x08))
#define QUADSPI_FCR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x0C))
#define QUADSPI_DLR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x10))
#define QUADSPI_CCR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x14))
#define QUADSPI_AR              (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x18))
#define QUADSPI_ABR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x1C))
#define QUADSPI_DR              (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x20))
#define QUADSPI_DR_BYTE         (*(volatile U8  *)(QUADSPI_BASE_ADDR + 0x20))
#define QUADSPI_PSMKR           (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x24))
#define QUADSPI_PSMAR           (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x28))
#define QUADSPI_PIR             (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x2C))
#define QUADSPI_LPTR            (*(volatile U32 *)(QUADSPI_BASE_ADDR + 0x30))

/*********************************************************************
*
*       Port B registers
*/
#define GPIOB_BASE_ADDR         0x40020400uL
#define GPIOB_MODER             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_OSPEEDR           (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x08))
#define GPIOB_PUPDR             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x0C))
#define GPIOB_ODR               (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x14))
#define GPIOB_AFRL              (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Port D registers
*/
#define GPIOD_BASE_ADDR         0x40020C00uL
#define GPIOD_MODER             (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x00))
#define GPIOD_OSPEEDR           (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x08))
#define GPIOD_PUPDR             (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x0C))
#define GPIOD_ODR               (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x14))
#define GPIOD_AFRL              (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x20))
#define GPIOD_AFRH              (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port E registers
*/
#define GPIOE_BASE_ADDR         0x40021000uL
#define GPIOE_MODER             (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x00))
#define GPIOE_OSPEEDR           (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x08))
#define GPIOE_PUPDR             (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x0C))
#define GPIOE_ODR               (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x14))
#define GPIOE_AFRL              (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x20))
#define GPIOE_AFRH              (*(volatile U32 *)(GPIOE_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port F registers
*/
#define GPIOF_BASE_ADDR         0x40021400uL
#define GPIOF_MODER             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x00))
#define GPIOF_OSPEEDR           (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x08))
#define GPIOF_PUPDR             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x0C))
#define GPIOF_ODR               (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x14))
#define GPIOF_AFRL              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x20))
#define GPIOF_AFRH              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port G registers
*/
#define GPIOG_BASE_ADDR         0x40021800uL
#define GPIOG_MODER             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x00))
#define GPIOG_OSPEEDR           (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x08))
#define GPIOG_PUPDR             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x0C))
#define GPIOG_ODR               (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x14))
#define GPIOG_AFRL              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x20))
#define GPIOG_AFRH              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR           0x40023800uL
#define RCC_AHB1RSTR            (*(volatile U32*)(RCC_BASE_ADDR + 0x10))
#define RCC_APB3RSTR            (*(volatile U32*)(RCC_BASE_ADDR + 0x18))
#define RCC_AHB1ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x30))
#define RCC_APB3ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x38))

/*********************************************************************
*
*       DMA 2 registers
*/
#define DMA2_BASE_ADDR          0x40026400uL
#define DMA2_STREAM_INDEX       7
#define DMA2_LISR               (*(volatile U32 *)(DMA2_BASE_ADDR + 0))
#define DMA2_HISR               (*(volatile U32 *)(DMA2_BASE_ADDR + 4))
#define DMA2_LIFCR              (*(volatile U32 *)(DMA2_BASE_ADDR + 8))
#define DMA2_HIFCR              (*(volatile U32 *)(DMA2_BASE_ADDR + 12))
#define DMA2_S7CR               (*(volatile U32 *)(DMA2_BASE_ADDR + 24 * DMA2_STREAM_INDEX + 16))
#define DMA2_S7NDTR             (*(volatile U32 *)(DMA2_BASE_ADDR + 24 * DMA2_STREAM_INDEX + 20))
#define DMA2_S7PAR              (*(volatile U32 *)(DMA2_BASE_ADDR + 24 * DMA2_STREAM_INDEX + 24))
#define DMA2_S7M0AR             (*(volatile U32 *)(DMA2_BASE_ADDR + 24 * DMA2_STREAM_INDEX + 28))
#define DMA2_S7FCR              (*(volatile U32 *)(DMA2_BASE_ADDR + 24 * DMA2_STREAM_INDEX + 36))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB1ENR_GPIOBEN         1
#define AHB1ENR_GPIODEN         3
#define AHB1ENR_GPIOEEN         4
#define AHB1ENR_GPIOFEN         5
#define AHB1ENR_GPIOGEN         6
#define AHB1ENR_DMA2EN          22
#define APB3ENR_QSPIEN          1

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_NCS_BIT             6   // Port G
#define NOR_CLK_BIT             2   // Port B
#define NOR_D0_BIT              8   // Port F
#define NOR_D1_BIT              9   // Port F
#define NOR_D2_BIT              2   // Port E
#define NOR_D3_BIT              13  // Port D

/*********************************************************************
*
*       GPIO bits and masks
*/
#define OSPEEDR_HIGH            2uL
#define OSPEEDR_MASK            0x3uL
#define MODER_MASK              0x3uL
#define MODER_ALT               2uL
#define AFR_MASK                0x3uL

/*********************************************************************
*
*       Quad-SPI control register
*/
#define CR_EN_BIT               0
#define CR_ABORT_BIT            1
#define CR_DMAEN_BIT            2
#define CR_FTHRES_MASK          0x1Fu
#define CR_FTHRES_BIT           8
#define CR_TRIE_BIT             16
#define CR_TCIE_BIT             17
#define CR_FTIE_BIT             18
#define CR_SMIE_BIT             19
#define CR_TOIE_BIT             20
#define CR_APMS_BIT             22
#define CR_PRESCALER_BIT        24
#define CR_PRESCALER_MAX        255

/*********************************************************************
*
*       Quad-SPI device configuration register
*/
#define DCR_CKMODE_BIT          0
#define DCR_FSIZE_BIT           16
#define DCR_FSIZE_MAX           0x1FuL

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
#define SR_FLEVEL_MASK          0x3FuL

/*********************************************************************
*
*       Quad-SPI communication configuration register
*/
#define CCR_INTRUCTION_BIT      0
#define CCR_IMODE_BIT           8
#define CCR_MODE_NONE           0
#define CCR_MODE_SINGLE         1uL
#define CCR_MODE_DUAL           2uL
#define CCR_MODE_QUAD           3uL
#define CCR_ADMODE_BIT          10
#define CCR_ADSIZE_BIT          12
#define CCR_ADSIZE_MASK         0x03uL
#define CCR_ABMODE_BIT          14
#define CCR_ABSIZE_BIT          16
#define CCR_ABSIZE_MASK         0x03uL
#define CCR_DCYC_BIT            18
#define CCR_DMODE_BIT           24
#define CCR_FMODE_BIT           26
#define CCR_FMODE_WRITE         0x0uL
#define CCR_FMODE_READ          0x1uL
#define CCR_FMODE_POLL          0x2uL
#define CCR_FMODE_MMAP          0x3uL
#define CCR_FMODE_MASK          0x3uL

/*********************************************************************
*
*       DMA2 related defines
*/
#define HIFCR_CFEIF7_BIT        22
#define HIFCR_CDMEIF7_BIT       24
#define HIFCR_CTCIF7_BIT        25
#define HIFCR_CHTIF7_BIT        26
#define HIFCR_CTEIF7_BIT        27
#define HISR_FEIF7_BIT          22
#define HISR_TEIF7_BIT          25
#define HISR_TCIF7_BIT          27
#define S7CR_EN_BIT             0
#define S7CR_TEIE_BIT           2
#define S7CR_TCIE_BIT           4
#define S7CR_DIR_M2P            1uL
#define S7CR_DIR_BIT            6
#define S7CR_PINC_BIT           9
#define S7CR_MINC_BIT           10
#define S7CR_PSIZE_32BIT        2uL
#define S7CR_PSIZE_BIT          11
#define S7CR_MSIZE_32BIT        2uL
#define S7CR_MSIZE_BIT          13
#define S7CR_PRIO_HIGH          3uL
#define S7CR_PRIO_BIT           16
#define S7CR_CHSEL_QUADSPI      3
#define S7CR_CHSEL_BIT          25
#define S7CR_PBURST_INCR4       1uL
#define S7CR_PBURST_BIT         21
#define S7CR_MBURST_INCR4       1uL
#define S7CR_MBURST_NONE        0uL
#define S7CR_MBURST_BIT         23

/*********************************************************************
*
*       Misc. defines
*/
#define NUM_BYTES_FIFO          32
#define CCR_DC_BIT              16
#define QUADSPI_PRIO            15
#define QUADSPI_MEM_SIZE_SHIFT  28                // Size of the memory region reserved for QUADSPI (256 MB)
#define DMA2_PRIO               15
#define WAIT_TIMEOUT_MS         1000
#define CYCLES_PER_MS           100               // Blocks the execution for about 1ms on a CPU running at 100 Mhz.
#define WAIT_TIMEOUT_CYCLES     (WAIT_TIMEOUT_MS * CYCLES_PER_MS)

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
static U8 _CalcClockDivider(U32 * pFreq_Hz) {
  U8  Div;
  U32 Freq_Hz;
  U32 MaxFreq_Hz;

  Div        = 0;
  MaxFreq_Hz = *pFreq_Hz;
  for (;;) {
    Freq_Hz = FS_NOR_HW_PER_CLK_HZ / (Div + 1);
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
  DMA2_S7CR   &= ~(1uL << S7CR_EN_BIT);                   // Stop the data transfer.
  while ((DMA2_S7CR & (1uL << S7CR_EN_BIT)) != 0u) {      // Wait for the stream to switch off.
    ;
  }
#if FS_NOR_HW_USE_DMA_MEM_PORT
  DMA2_S7M0AR = (U32)&QUADSPI_DR;
  DMA2_S7PAR  = (U32)pData;
#else
  DMA2_S7PAR  = (U32)&QUADSPI_DR;
  DMA2_S7M0AR = (U32)pData;
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
  DMA2_S7NDTR = NumBytes >> 2;                            // We transfer 4 bytes at a time.
  DMA2_HIFCR  = 0                                         // Clear any pending interrupts.
              | (1uL << HIFCR_CDMEIF7_BIT)
              | (1uL << HIFCR_CTEIF7_BIT)
              | (1uL << HIFCR_CHTIF7_BIT)
              | (1uL << HIFCR_CTCIF7_BIT)
              | (1uL << HIFCR_CFEIF7_BIT)
              ;
  DMA2_S7CR   = 0
              | (S7CR_PSIZE_32BIT   << S7CR_PSIZE_BIT)    // Peripheral bus width
              | (S7CR_MSIZE_32BIT   << S7CR_MSIZE_BIT)    // Memory bus width
#if FS_NOR_HW_USE_DMA_MEM_PORT
              | (1uL                << S7CR_PINC_BIT)     // Peripheral increment enable
#else
              | (1uL                << S7CR_MINC_BIT)     // Memory increment enable
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
              | (S7CR_PRIO_HIGH     << S7CR_PRIO_BIT)     // Set priority to high
              | (S7CR_CHSEL_QUADSPI << S7CR_CHSEL_BIT)    // Channel connected to QUADSPI peripheral
              ;
#if FS_NOR_HW_USE_DMA_MEM_PORT
  if (IsWrite == 0) {
#else
  if (IsWrite != 0) {
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
    DMA2_S7CR |= S7CR_DIR_M2P << S7CR_DIR_BIT;
  }
#if FS_NOR_HW_USE_OS
  DMA2_S7CR   |= 0
              | (1uL << S7CR_TEIE_BIT)
              | (1uL << S7CR_TCIE_BIT)
              ;
  _StatusDMA  = 0;
#endif // FS_NOR_HW_USE_OS
#if (FS_NOR_HW_USE_DMA_MEM_PORT == 0)
  QUADSPI_CR  |= 0u
              | ((4u - 1u) << CR_FTHRES_BIT)              // Signal the DMA when 4 bytes are available or free.
              | (1uL       << CR_DMAEN_BIT)               // Enable the data transfer via DMA in QSPI controller.
              ;
#endif // FS_NOR_HW_USE_DMA_MEM_PORT == 0
  DMA2_S7CR   |= 1uL << S7CR_EN_BIT;                      // Start the data transfer
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
    if ((StatusDMA & (1uL << HISR_TEIF7_BIT)) != 0u) {
      break;                                              // Error, could not transfer data via DMA.
    }
    if ((StatusQSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusQSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << HISR_TCIF7_BIT)) != 0) {
#if FS_NOR_HW_USE_DMA_MEM_PORT
      //
      // It looks like the DMA does not transfer the last
      // 4 bytes read by the QUADSPI controller which means
      // that the QUADSPI controller remains in busy state
      // forever. We check here for this condition and if
      // true we read and discard the data so that the
      // QUADSPI controller can perform the next operation.
      // We know that we have to read only for 4 bytes because
      // the DMA is configured to work in direct mode and in
      // this mode the DMA can buffer only 4 bytes.
      //
      U32 Status;

      Status = QUADSPI_SR;
      if ((Status & (SR_FLEVEL_MASK << SR_FLEVEL_BIT)) != 0) {
        (void)QUADSPI_DR;
      }
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
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
    StatusDMA  = DMA2_HISR;
    if ((StatusDMA & (1uL << HISR_TEIF7_BIT)) != 0u) {
      break;                                              // Error, could not transfer data via DMA.
    }
    if ((StatusQSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusQSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << HISR_TCIF7_BIT)) != 0) {
      r = 0;                                              // OK, data transferred.
      break;
    }
    if (--TimeOut == 0) {
      break;                                              // Error, a timeout occurred.
    }
  }
#endif // FS_NOR_HW_USE_OS
  if (r != 0) {
    DMA2_S7CR = 0;                                        // Stop the data transfer in case of an error.
    while ((DMA2_S7CR & (1uL << S7CR_EN_BIT)) != 0u) {    // Wait for the stream to switch off
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
  U32 Div;
  U32 Freq_Hz;

  FS_USE_PARA(Unit);
  //
  // Enable the clocks of peripherals and reset them.
  //
  RCC_AHB1ENR  |= 0
               | (1uL << AHB1ENR_GPIOBEN)
               | (1uL << AHB1ENR_GPIODEN)
               | (1uL << AHB1ENR_GPIOEEN)
               | (1uL << AHB1ENR_GPIOFEN)
               | (1uL << AHB1ENR_GPIOGEN)
#if FS_NOR_HW_USE_DMA
               | (1uL << AHB1ENR_DMA2EN)
#endif
               ;
  RCC_APB3ENR  |=   1uL << APB3ENR_QSPIEN;
  RCC_APB3RSTR |=   1uL << APB3ENR_QSPIEN;
  RCC_APB3RSTR &= ~(1uL << APB3ENR_QSPIEN);
  //
  // Wait for the unit to exit reset.
  //
  for (;;) {
    if ((RCC_APB3RSTR & (1uL << APB3ENR_QSPIEN)) == 0) {
      break;
    }
  }
  //
  // NCS is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << (NOR_NCS_BIT << 1));
  GPIOG_MODER   |=   MODER_ALT    << (NOR_NCS_BIT << 1);
  GPIOG_AFRL    &= ~(AFR_MASK     << (NOR_NCS_BIT << 2));
  GPIOG_AFRL    |=   0xAuL        << (NOR_NCS_BIT << 2);
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_NCS_BIT << 1));
  GPIOG_OSPEEDR |=   OSPEEDR_HIGH << (NOR_NCS_BIT << 1);
  //
  // CLK is an output signal controlled by the QUADSPI unit.
  //
  GPIOB_MODER   &= ~(MODER_MASK   << (NOR_CLK_BIT << 1));
  GPIOB_MODER   |=   MODER_ALT    << (NOR_CLK_BIT << 1);
  GPIOB_AFRL    &= ~(AFR_MASK     << (NOR_CLK_BIT << 2));
  GPIOB_AFRL    |=   0x9uL        << (NOR_CLK_BIT << 2);
  GPIOB_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_CLK_BIT << 1));
  GPIOB_OSPEEDR |=   OSPEEDR_HIGH << (NOR_CLK_BIT << 1);
  //
  // D0 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_D0_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_D0_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_D0_BIT - 8) << 2));
  GPIOF_AFRH    |=   0xAuL        << ((NOR_D0_BIT - 8) << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D0_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D0_BIT << 1);
  //
  // D1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_D1_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_D1_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_D1_BIT - 8) << 2));
  GPIOF_AFRH    |=   0xAuL        << ((NOR_D1_BIT - 8) << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D1_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D1_BIT << 1);
  //
  // D2 is an input/output signal and is controlled by the QUADSPI unit.
  //
  GPIOE_MODER   &= ~(MODER_MASK   << (NOR_D2_BIT << 1));
  GPIOE_MODER   |=   MODER_ALT    << (NOR_D2_BIT << 1);
  GPIOE_AFRL    &= ~(AFR_MASK     << (NOR_D2_BIT << 2));
  GPIOE_AFRL    |=   0x9uL        << (NOR_D2_BIT << 2);
  GPIOE_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D2_BIT << 1));
  GPIOE_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D2_BIT << 1);
  //
  // D3 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOD_MODER   &= ~(MODER_MASK   << (NOR_D3_BIT << 1));
  GPIOD_MODER   |=   MODER_ALT    << (NOR_D3_BIT << 1);
  GPIOD_AFRH    &= ~(AFR_MASK     << ((NOR_D3_BIT - 8) << 2));
  GPIOD_AFRH    |=   0x9uL        << ((NOR_D3_BIT - 8) << 2);
  GPIOD_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D3_BIT << 1));
  GPIOD_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D3_BIT << 1);
#if FS_NOR_HW_USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(QUADSPI_IRQn);
  NVIC_SetPriority(QUADSPI_IRQn, QUADSPI_PRIO);
#if FS_NOR_HW_USE_DMA
  NVIC_DisableIRQ(DMA2_Stream7_IRQn);
  NVIC_SetPriority(DMA2_Stream7_IRQn, DMA2_PRIO);
#endif // FS_NOR_HW_USE_DMA
#endif
  //
  // Initialize the Quad-SPI controller.
  //
  Freq_Hz = FS_NOR_HW_NOR_CLK_HZ;
  Div = (U32)_CalcClockDivider(&Freq_Hz);
  QUADSPI_CR  = 0
              | (1uL << CR_EN_BIT)                  // Enable the Quad-SPI unit.
              | (Div << CR_PRESCALER_BIT)
              ;
  QUADSPI_DCR = 0
              | (1uL           << DCR_CKMODE_BIT)   // CLK signals stays HIGH when the NOR flash is not selected.
              | (DCR_FSIZE_MAX << DCR_FSIZE_BIT)    // We set the NOR flash size to maximum since this value is not known at this stage.
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
  NVIC_EnableIRQ(DMA2_Stream7_IRQn);
#endif // FS_NOR_HW_USE_DMA
#endif
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
  DataMode       = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), 1);                               // We read at least one byte.
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
  while (1) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  _ClearFlags();
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
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0u) {
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
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0u) {
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
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0u) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
  NumBytesToRead = NumBytesData - 1;        // 0 means "read 1 byte".
#if FS_NOR_HW_USE_DMA
  if (NumBytesData != 0u) {
    if (_IsDataAligned(pData, NumBytesData) != 0) {
#if FS_NOR_HW_USE_DMA_MEM_PORT
      //
      // According to section "4.9.1 Inverting transfers over DMA2 AHB ports consideration"
      // in [5] more data bytes have to be read in order to make sure that the DMA FIFO
      // is emptied on the last data transfer. In addition, the DMA transfer has to be started
      // after the QADSPI controller has been configured.
      //
      NumBytesToRead += 4;                  // In direct mode the DMA can queue a maximum of 4 bytes.
      QUADSPI_CR &= ~(CR_FTHRES_MASK << CR_FTHRES_BIT);
      QUADSPI_CR |= 1uL << CR_DMAEN_BIT;
#else
      _StartDMATransfer((U32 *)pData, NumBytesData, 0);
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
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
#if FS_NOR_HW_USE_DMA_MEM_PORT
      _StartDMATransfer((U32 *)pData, NumBytesData, 0);
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
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
  _ClearFlags();
#if FS_NOR_HW_USE_DMA
  if (NumBytesData != 0u) {
    if (_IsDataAligned(pData, NumBytesData) != 0) {
#if FS_NOR_HW_USE_DMA_MEM_PORT
      QUADSPI_CR &= ~(CR_FTHRES_MASK << CR_FTHRES_BIT);
      QUADSPI_CR |= 1uL << CR_DMAEN_BIT;
#endif // FS_NOR_HW_USE_DMA_MEM_PORT
    }
  }
#endif // FS_NOR_HW_USE_DMA
  if (NumBytesData) {
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
  if (NumBytesData != 0u) {
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
      QUADSPI_DLR = NumBytes - 1;           // 0 means "read 1 byte".
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
*       Public code
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS

/*********************************************************************
*
*       QuadSPI_IRQHandler
*/
void QuadSPI_IRQHandler(void) {
#if (FS_NOR_HW_USE_OS == 1)
  OS_EnterNestableInterrupt();
#endif
  _StatusQSPI = QUADSPI_SR;
  QUADSPI_FCR = 0                    // Prevent other interrupts from occurring.
              | (1uL << SR_SMF_BIT)
              | (1uL << SR_TOF_BIT)
              | (1uL << SR_FTF_BIT)
              | (1uL << SR_TCF_BIT)
              | (1uL << SR_TEF_BIT)
              ;
  FS_X_OS_Signal();
#if (FS_NOR_HW_USE_OS == 1)
  OS_LeaveNestableInterrupt();
#endif
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       DMA2_Stream7_IRQHandler
*/
void DMA2_Stream7_IRQHandler(void) {
  U32 Status;

#if (FS_NOR_HW_USE_OS == 1)
  OS_EnterNestableInterrupt();
#endif
  Status      = DMA2_HISR;
  Status     &= 0               // Make sure that we clear only the flags assigned to the DMA stream we use.
             | (1uL << HIFCR_CFEIF7_BIT)
             | (1uL << HIFCR_CDMEIF7_BIT)
             | (1uL << HIFCR_CTCIF7_BIT)
             | (1uL << HIFCR_CHTIF7_BIT)
             | (1uL << HIFCR_CTEIF7_BIT)
             ;
  DMA2_HIFCR  = Status;         // Clear pending interrupt flags.
  _StatusDMA |= Status;         // Save the status to a static variable and check it in the task.
  FS_X_OS_Signal();             // Wake up the task.
#if (FS_NOR_HW_USE_OS == 1)
  OS_LeaveNestableInterrupt();
#endif
}

#endif // FS_NOR_HW_USE_DMA

#endif // FS_NOR_HW_USE_OS

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32F413_ST_32F413HDISCOVERY = {
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
