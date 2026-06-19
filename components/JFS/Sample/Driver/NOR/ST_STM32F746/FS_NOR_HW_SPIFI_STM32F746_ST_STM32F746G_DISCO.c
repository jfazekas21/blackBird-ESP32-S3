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

File      : FS_NOR_HW_SPIFI_STM32F746_ST_STM32F746G_DISCO.c
Purpose   : Low-level flash driver for STM32F7 QSPI interface.
Literature:
  [1] RM0385 Reference manual STM32F75xxx and STM32F74xxx advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\RM0385_STM32F74xxx_STM32F75xxx_Rev5_DM00124865.pdf)
  [2] STM32F74xxx STM32F75xxx Errata sheet
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\STM32F74xxx_STM32F75xxx_Errata_Rev3.pdf)
  [3] UM1907 User manual Discovery kit for STM32F7 Series with STM32F746NG MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32F7\EvalBoard\STM32F746_Discovery\DM00190424.pdf)
*/

/*********************************************************************
*
*      #include section
*
**********************************************************************
*/
#include <stddef.h>
#include "FS.h"

/*********************************************************************
*
*      Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_HW_PER_CLK_HZ
  #define FS_NOR_HW_PER_CLK_HZ              216000000         // Clock of Quad-SPI unit
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ              25000000          // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_QUADSPI_MEM_ADDR
  #define FS_NOR_HW_QUADSPI_MEM_ADDR        0x90000000        // This is the start address of the memory region used by the file system
                                                              // to read the data from the serial NOR flash device. The hardware layer
                                                              // uses this address to invalidate the data cache. It should be set to the value passed
                                                              // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS                  0                 // Enables/disables the interrupt-driven operation.
                                                              // Permitted values:
                                                              //   0 - polling via CPU
                                                              //   1 - event-driven using embOS
                                                              //   2 - event-driven using other RTOS
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "stm32f7xx.h"
  #include "FS_OS.h"
#if (FS_NOR_HW_USE_OS == 1)
  #include "RTOS.h"
#endif // FS_NOR_HW_USE_OS == 1
#endif // FS_NOR_HW_USE_OS

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
*       System Control Block
*/
#define SCB_BASE_ADDR           0xE000E000uL
#define SCB_CCR                 (*(volatile U32 *)(SCB_BASE_ADDR + 0xD14))    // Configuration Control Register

/*********************************************************************
*
*       Memory protection unit
*/
#define MPU_BASE_ADDR           0xE000ED90uL
#define MPU_TYPE                (*(volatile U32 *)(MPU_BASE_ADDR + 0x00))
#define MPU_CTRL                (*(volatile U32 *)(MPU_BASE_ADDR + 0x04))
#define MPU_RNR                 (*(volatile U32 *)(MPU_BASE_ADDR + 0x08))
#define MPU_RBAR                (*(volatile U32 *)(MPU_BASE_ADDR + 0x0C))
#define MPU_RASR                (*(volatile U32 *)(MPU_BASE_ADDR + 0x10))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB1ENR_GPIOBEN         1
#define AHB1ENR_GPIODEN         3
#define AHB1ENR_GPIOEEN         4
#define APB3ENR_QSPIEN          1

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_NCS_BIT             6   // Port B
#define NOR_CLK_BIT             2   // Port B
#define NOR_D0_BIT              11  // Port D
#define NOR_D1_BIT              12  // Port D
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
#define CR_SMIE_BIT             19
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
*       MPU defines
*/
#define CTRL_ENABLE_BIT         0
#define CTRL_PRIVDEFENA_BIT     2
#define RNR_REGION_MASK         0xFF
#define RASR_ENABLE_BIT         0
#define RASR_SIZE_BIT           1
#define RASR_TEX_BIT            19
#define RASR_AP_BIT             24
#define RASR_XN_BIT             28
#define RASR_AP_FULL            0x3uL
#define TYPE_DREGION_BIT        8
#define TYPE_DREGION_MASK       0xFFuL
#define RBAR_REGION_BIT         0
#define RBAR_REGION_MASK        0x1FuL
#define RBAR_VALID_BIT          5

/*********************************************************************
*
*       Misc. defines
*/
#define NUM_BYTES_FIFO          32
#define CCR_DC_BIT              16
#define QUADSPI_PRIO            15
#define QUADSPI_MEM_SIZE_SHIFT  28


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
  while (1) {
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
  while (1) {
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
  while (1) {
    if ((QUADSPI_CR & (1uL << CR_ABORT_BIT)) == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _DisableDCache
*/
static void _DisableDCache(void) {
  U32 NumRegions;
  U32 iRegion;
  U32 Mask;

  if (SCB_CCR & (1uL << CCR_DC_BIT)) {      // Is data cache enabled?
    NumRegions = (MPU_TYPE >> TYPE_DREGION_BIT) & TYPE_DREGION_MASK;
    //
    // Find the next free region.
    //
    for (iRegion = 0; iRegion < NumRegions; ++iRegion) {
      MPU_RNR = iRegion;
      if ((MPU_RASR & (1uL << RASR_ENABLE_BIT)) == 0) {
        break;            // Found a free region.
      }
      //
      // Check if we already configured the MPU.
      //
      Mask = 0uL
           | (RBAR_REGION_MASK << RBAR_REGION_BIT)
           | (1uL              << RBAR_VALID_BIT)
           ;
      if ((MPU_RBAR & ~Mask) == FS_NOR_HW_QUADSPI_MEM_ADDR) {
        iRegion = NumRegions;
        break;            // Already configured.
      }
    }
    if (iRegion < NumRegions) {
      //
      // Use MPU to disable the data cache on the memory region assigned to QSPI.
      //
      MPU_CTRL &= ~(1uL << CTRL_ENABLE_BIT);      // Disable MPU first.
      MPU_RNR   = iRegion & RNR_REGION_MASK;
      MPU_RBAR  = FS_NOR_HW_QUADSPI_MEM_ADDR;
      MPU_RASR  = 0
                | (1uL                          << RASR_XN_BIT)
                | (RASR_AP_FULL                 << RASR_AP_BIT)
                | (1uL                          << RASR_TEX_BIT)
                | ((QUADSPI_MEM_SIZE_SHIFT - 1) << RASR_SIZE_BIT)
                | (1uL                          << RASR_ENABLE_BIT)
                ;
      //
      // Enable MPU.
      //
      MPU_CTRL |= 0
               | (1uL << CTRL_PRIVDEFENA_BIT)
               | (1uL << CTRL_ENABLE_BIT)
               ;
    }
  }
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
               ;
  RCC_APB3ENR  |=   1uL << APB3ENR_QSPIEN;
  RCC_APB3RSTR |=   1uL << APB3ENR_QSPIEN;
  RCC_APB3RSTR &= ~(1uL << APB3ENR_QSPIEN);
  //
  // Wait for the unit to exit reset.
  //
  while (1) {
    if ((RCC_APB3RSTR & (1uL << APB3ENR_QSPIEN)) == 0) {
      break;
    }
  }
  //
  // Disable the cache on the memory region assigned to QSPI.
  //
  _DisableDCache();
  //
  // NCS is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOB_MODER   &= ~(MODER_MASK   << (NOR_NCS_BIT << 1));
  GPIOB_MODER   |=   MODER_ALT    << (NOR_NCS_BIT << 1);
  GPIOB_AFRL    &= ~(AFR_MASK     << (NOR_NCS_BIT << 2));
  GPIOB_AFRL    |=   0xAuL        << (NOR_NCS_BIT << 2);
  GPIOB_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_NCS_BIT << 1));
  GPIOB_OSPEEDR |=   OSPEEDR_HIGH << (NOR_NCS_BIT << 1);
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
  GPIOD_MODER   &= ~(MODER_MASK   << (NOR_D0_BIT << 1));
  GPIOD_MODER   |=   MODER_ALT    << (NOR_D0_BIT << 1);
  GPIOD_AFRH    &= ~(AFR_MASK     << ((NOR_D0_BIT - 8) << 2));
  GPIOD_AFRH    |=   0x9uL        << ((NOR_D0_BIT - 8) << 2);
  GPIOD_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D0_BIT << 1));
  GPIOD_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D0_BIT << 1);
  //
  // D1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOD_MODER   &= ~(MODER_MASK   << (NOR_D1_BIT << 1));
  GPIOD_MODER   |=   MODER_ALT    << (NOR_D1_BIT << 1);
  GPIOD_AFRH    &= ~(AFR_MASK     << ((NOR_D1_BIT - 8) << 2));
  GPIOD_AFRH    |=   0x9uL        << ((NOR_D1_BIT - 8) << 2);
  GPIOD_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_D1_BIT << 1));
  GPIOD_OSPEEDR |=   OSPEEDR_HIGH << (NOR_D1_BIT << 1);
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
#endif // FS_NOR_HW_USE_OS
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
              | (1uL << CR_SMIE_BIT)
              | (1uL << CR_APMS_BIT)
              ;
  QUADSPI_FCR  = 1uL << SR_SMF_BIT;
  NVIC_EnableIRQ(QUADSPI_IRQn);
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
  while (1) {
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
  while (1) {
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
  U32 DataReg;
  U32 AltSize;
  U32 AddrSize;
  U32 NumBytesAvail;
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
  while (1) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
  if (NumBytesData) {
    QUADSPI_DLR = NumBytesData - 1;       // 0 means "read 1 byte".
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    QUADSPI_AR = AddrReg;
  }
  //
  // Read data from NOR flash.
  //
  if (NumBytesData) {
    do {
      //
      // Wait for the data to be received.
      //
      while (1) {
        NumBytesAvail = (QUADSPI_SR >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
        if ((NumBytesAvail >= 4) || (NumBytesAvail >= NumBytesData)) {
          break;
        }
      }
      //
      // Read data and store it to destination buffer.
      //
      if (NumBytesData < 4) {
        //
        // Read single bytes.
        //
        do {
          *pData++ = QUADSPI_DR_BYTE;
        } while (--NumBytesData);
      } else {
        //
        // Read 4 bytes at a time.
        //
        DataReg = QUADSPI_DR;
        *pData++ = (U8)DataReg;
        *pData++ = (U8)(DataReg >> 8);
        *pData++ = (U8)(DataReg >> 16);
        *pData++ = (U8)(DataReg >> 24);
        NumBytesData -= 4;
      }
    } while (NumBytesData);
  }
  //
  // Wait until the data transfer has been completed.
  //
  while (1) {
    if (QUADSPI_SR & (1uL << SR_TCF_BIT)) {
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
  U32 AddrReg;
  U32 AltReg;
  U32 CfgReg;
  U32 DataMode;
  U32 AddrMode;
  U32 AltMode;
  U32 CmdMode;
  U32 DataReg;
  U32 AddrSize;
  U32 AltSize;
  U32 NumBytes;
  U32 NumBytesAlt;
  U32 NumBytesFree;

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
  while (1) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
  if (NumBytesData) {
    QUADSPI_DLR = NumBytesData - 1;       // 0 means "read 1 byte".
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    QUADSPI_AR = AddrReg;
  }
  //
  // write data to NOR flash.
  //
  if (NumBytesData) {
    do {
      //
      // Wait for free space in FIFO.
      //
      while (1) {
        NumBytesFree = (QUADSPI_SR >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
        NumBytesFree = NUM_BYTES_FIFO - NumBytesFree;
        if ((NumBytesFree >= 4) || (NumBytesFree >= NumBytesData)) {
          break;
        }
      }
      //
      // Get the data from source buffer and write it.
      //
      if (NumBytesData < 4) {
        //
        // Write single bytes.
        //
        do {
          QUADSPI_DR_BYTE = *pData++;
        } while (--NumBytesData);
      } else {
        //
        // Write 4 bytes at a time if possible.
        //
        DataReg  = (U32)*pData++;
        DataReg |= (U32)*pData++ << 8;
        DataReg |= (U32)*pData++ << 16;
        DataReg |= (U32)*pData++ << 24;
        NumBytesData -= 4;
        QUADSPI_DR = DataReg;
      }
    } while (NumBytesData);
  }
  //
  // Wait until the data transfer has been completed.
  //
  while (1) {
    if (QUADSPI_SR & (1uL << SR_TCF_BIT)) {
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
  r = -1;                 // Set to indicate that the feature is not supported.
#if FS_NOR_HW_USE_OS
  {
    U32 CfgReg;
    U32 DataMode;
    U32 CmdMode;
    U32 NumBytes;
    U32 DataMask;
    U32 DataMatch;

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
    while (1) {
      if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
        break;
      }
    }
    //
    // Start the poll operation. The end of operation is signaled by interrupt.
    //
    _ClearFlags();
    if (NumBytes) {
      QUADSPI_DLR = NumBytes - 1;           // 0 means "read 1 byte".
    }
    QUADSPI_PSMKR = DataMask;
    QUADSPI_PSMAR = DataMatch;
    QUADSPI_PIR   = Delay;
    QUADSPI_FCR   = 1uL << SR_SMF_BIT;
    QUADSPI_CCR   = CfgReg;
    r = FS_X_OS_Wait(TimeOut_ms);
    if (r) {
      //
      // The timeout has expired. Cancel the poll operation.
      //
      _Cancel();
      r = 1;                                // A timeout occurred.
    } else {
      r = 0;                                // Bit set to the requested value.
    }
  }
#else
  FS_USE_PARA(Cmd);
  FS_USE_PARA(BitPos);
  FS_USE_PARA(BitValue);
  FS_USE_PARA(Delay);
  FS_USE_PARA(TimeOut_ms);
  FS_USE_PARA(BusWidth);
#endif // FS_NOR_HW_USE_OS
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
#endif  // FS_NOR_HW_USE_OS
  return r;
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
*       QUADSPI_IRQHandler
*/
void QUADSPI_IRQHandler(void);
void QUADSPI_IRQHandler(void) {
#if (FS_NOR_HW_USE_OS == 1)
  OS_EnterInterrupt();
#endif // FS_NOR_HW_USE_OS == 1
  QUADSPI_FCR = 1uL << SR_SMF_BIT;    // Prevent other interrupts from occurring.
  FS_X_OS_Signal();
#if (FS_NOR_HW_USE_OS == 1)
  OS_LeaveInterrupt();
#endif // FS_NOR_HW_USE_OS == 1
}

#endif // FS_NOR_HW_USE_OS

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32F746_ST_STM32F746G_DISCO = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
  _HW_Poll,
  _HW_Delay,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
