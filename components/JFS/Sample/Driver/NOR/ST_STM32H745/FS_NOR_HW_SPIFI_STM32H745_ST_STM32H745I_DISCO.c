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

File      : FS_NOR_HW_SPIFI_STM32H745_ST_STM32H745_DISCO.c
Purpose   : Low-level flash driver for STM32H7 QSPI interface.
Literature:
  [1] RM0399 Reference manual STM32H745/755 and STM32H747/757 advanced Arm-based 32-bit MCUs
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32H7\RM0399_STM32H745_H747_H755_H757.pdf)
  [2] Data sheet STM32H745xI/G
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32H7\DS12923_STM32H745x_rev1.pdf)
  [3] Errata sheet STM32H745/747/755/757xx
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32H7\ES0445_rev1_STM32H745_747XIG_STM32H755_757XI_DM00530531.pdf)
  [4] UM2488 Discovery kits with STM32H745XI and STM32H750XB microcontrollers
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32H7\EvalBoard\STM32H745I_DISCO\en.DM00547983.pdf)

Additional information:
  Pin assignment of the QSPI interface

  Name          Port pin
  ----------------------
  QSPI_CLK      PF10
  QSPI_BK1_NCS  PG6
  QSPI_BK2_NCS  PC11
  QSPI_BK1_IO0  PD11
  QSPI_BK1_IO1  PF9
  QSPI_BK1_IO2  PF7
  QSPI_BK1_IO3  PF6
  QSPI_BK2_IO0  PH2
  QSPI_BK2_IO1  PH3
  QSPI_BK2_IO2  PG9
  QSPI_BK2_IO3  PG14
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
#ifndef   FS_NOR_HW_QUADSPI_CLK_HZ
  #define FS_NOR_HW_QUADSPI_CLK_HZ            240000000     // Clock of Quad-SPI unit
#endif

#ifndef   FS_NOR_HW_FLASH_CLK_HZ
  #define FS_NOR_HW_FLASH_CLK_HZ              60000000      // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_QUADSPI_MEM_ADDR
  #define FS_NOR_HW_QUADSPI_MEM_ADDR          0x90000000    // This is the start address of the memory region used by the file system
                                                            // to read the data from the serial NOR flash device. The hardware layer
                                                            // uses this address to invalidate the data cache. It should be set to the value passed
                                                            // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

#ifndef   FS_NOR_HW_USE_DUAL_FLASH_MODE
  #define FS_NOR_HW_USE_DUAL_FLASH_MODE       1             // Set to 1 if two identical NOR flash devices are connected.
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       QSPI registers
*/
#define QUADSPI_BASE_ADDR       0x52005000uL
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
#define GPIOB_BASE_ADDR         0x58020400uL
#define GPIOB_MODER             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_OSPEEDR           (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x08))
#define GPIOB_PUPDR             (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x0C))
#define GPIOB_ODR               (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x14))
#define GPIOB_AFRL              (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Port C registers
*/
#define GPIOC_BASE_ADDR         0x58020800uL
#define GPIOC_MODER             (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x00))
#define GPIOC_OSPEEDR           (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x08))
#define GPIOC_PUPDR             (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x0C))
#define GPIOC_AFRL              (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x20))
#define GPIOC_AFRH              (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port D registers
*/
#define GPIOD_BASE_ADDR         0x58020C00uL
#define GPIOD_MODER             (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x00))
#define GPIOD_OSPEEDR           (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x08))
#define GPIOD_PUPDR             (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x0C))
#define GPIOD_AFRL              (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x20))
#define GPIOD_AFRH              (*(volatile U32 *)(GPIOD_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port F registers
*/
#define GPIOF_BASE_ADDR         0x58021400uL
#define GPIOF_MODER             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x00))
#define GPIOF_OSPEEDR           (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x08))
#define GPIOF_PUPDR             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x0C))
#define GPIOF_BSRR              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x18))
#define GPIOF_AFRL              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x20))
#define GPIOF_AFRH              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port G registers
*/
#define GPIOG_BASE_ADDR         0x58021800uL
#define GPIOG_MODER             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x00))
#define GPIOG_OSPEEDR           (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x08))
#define GPIOG_PUPDR             (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x0C))
#define GPIOG_BSRR              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x18))
#define GPIOG_AFRL              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x20))
#define GPIOG_AFRH              (*(volatile U32 *)(GPIOG_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port H registers
*/
#define GPIOH_BASE_ADDR         0x58021C00uL
#define GPIOH_MODER             (*(volatile U32 *)(GPIOH_BASE_ADDR + 0x00))
#define GPIOH_OSPEEDR           (*(volatile U32 *)(GPIOH_BASE_ADDR + 0x08))
#define GPIOH_PUPDR             (*(volatile U32 *)(GPIOH_BASE_ADDR + 0x0C))
#define GPIOH_ODR               (*(volatile U32 *)(GPIOH_BASE_ADDR + 0x14))
#define GPIOH_AFRL              (*(volatile U32 *)(GPIOH_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR           0x58024400uL
#define RCC_AHB3RSTR            (*(volatile U32*)(RCC_BASE_ADDR + 0x7C))
#define RCC_AHB3ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0xD4))
#define RCC_AHB4ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0xE0))

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
#define AHB1ENR_GPIOCEN         2
#define AHB1ENR_GPIODEN         3
#define AHB1ENR_GPIOFEN         5
#define AHB1ENR_GPIOGEN         6
#define AHB1ENR_GPIOHEN         7
#define AHB3ENR_QSPIEN          14

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_BK1_NCS_BIT         6   // Port G
#define NOR_BK2_NCS_BIT         11  // Port C
#define NOR_CLK_BIT             10  // Port F
#define NOR_BK1_D0_BIT          11  // Port D
#define NOR_BK1_D1_BIT          9   // Port F
#define NOR_BK1_D2_BIT          7   // Port F
#define NOR_BK1_D3_BIT          6   // Port F
#define NOR_BK2_D0_BIT          2   // Port H
#define NOR_BK2_D1_BIT          3   // Port H
#define NOR_BK2_D2_BIT          9   // Port G
#define NOR_BK2_D3_BIT          14  // Port G

/*********************************************************************
*
*       GPIO bits and masks
*/
#define OSPEEDR_HIGH            2uL
#define OSPEEDR_MASK            0x3uL
#define MODER_MASK              0x3uL
#define MODER_ALT               2uL
#define MODER_OUTPUT            1uL
#define AFR_MASK                0x3uL

/*********************************************************************
*
*       Quad-SPI control register
*/
#define CR_EN_BIT               0
#define CR_ABORT_BIT            1
#define CR_DFM_BIT              6
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
    Freq_Hz = FS_NOR_HW_QUADSPI_CLK_HZ / (Div + 1);
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

#ifndef CORE_M4

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

#endif // CORE_M4

/*********************************************************************
*
*       _ConfigWPIfRequired
*
*  Function description
*    Configures the pin connected to the write protect signal either
*    as pin assigned to QSPI controller.
*
*  Additional information
*    The QSPI controller sets the pin connected to the write protect
*    signal of the NOR flash device (IO2) to output mode and forced
*    to LOW during the single and dual data transfers. This prevents
*    the NOR driver from disabling the write protection of the physical
*    sectors via the status register. This pin is initially configured
*    as GPIO output set to HIGH in the _HW_Init() and is later assigned
*    to QSPI controller at the first data transfer via 4 data lines.
*/
static void _ConfigWPIfRequired(U16 BusWidth) {
  U8 IsQuadReq;
  U8 IsQuadConf;

  IsQuadReq = 0;
  if (   (FS_BUSWIDTH_GET_CMD(BusWidth)  == 4u)
      || (FS_BUSWIDTH_GET_ADDR(BusWidth) == 4u)
      || (FS_BUSWIDTH_GET_DATA(BusWidth) == 4u)) {
    IsQuadReq = 1;
  }
  IsQuadConf = 0;
  if (   ((GPIOF_MODER & (MODER_MASK << (NOR_BK1_D2_BIT << 1))) == (MODER_ALT << (NOR_BK1_D2_BIT << 1)))
      && ((GPIOG_MODER & (MODER_MASK << (NOR_BK2_D2_BIT << 1))) == (MODER_ALT << (NOR_BK2_D2_BIT << 1)))) {
    IsQuadConf = 1;
  }
  if (IsQuadReq != 0u) {
    if (IsQuadConf != IsQuadReq) {
      //
      // D2 of bank 1 is an input/output signal and is controlled by the QUADSPI unit.
      //
      GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D2_BIT << 1));
      GPIOF_MODER   |=   MODER_ALT    << (NOR_BK1_D2_BIT << 1);
      GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D2_BIT << 2));
      GPIOF_AFRL    |=   0x9uL        << (NOR_BK1_D2_BIT << 2);
      GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D2_BIT << 1));
      GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D2_BIT << 1);
      //
      // D2 of bank 2 is an input/output signal controlled by the QUADSPI unit.
      //
      GPIOG_MODER   &= ~(MODER_MASK   << (NOR_BK2_D2_BIT << 1));
      GPIOG_MODER   |=   MODER_ALT    << (NOR_BK2_D2_BIT << 1);
      GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_BK2_D2_BIT - 8) << 2));
      GPIOG_AFRH    |=   0x9uL        << ((NOR_BK2_D2_BIT - 8) << 2);
      GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK2_D2_BIT << 1));
      GPIOG_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK2_D2_BIT << 1);
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
  RCC_AHB4ENR  |= 0
               | (1uL << AHB1ENR_GPIOBEN)
               | (1uL << AHB1ENR_GPIOCEN)
               | (1uL << AHB1ENR_GPIODEN)
               | (1uL << AHB1ENR_GPIOFEN)
               | (1uL << AHB1ENR_GPIOGEN)
               | (1uL << AHB1ENR_GPIOHEN)
               ;
  RCC_AHB3ENR  |=   1uL << AHB3ENR_QSPIEN;
  RCC_AHB3RSTR |=   1uL << AHB3ENR_QSPIEN;
  RCC_AHB3RSTR &= ~(1uL << AHB3ENR_QSPIEN);
  //
  // Wait for the unit to exit reset.
  //
  while (1) {
    if ((RCC_AHB3RSTR & (1uL << AHB3ENR_QSPIEN)) == 0) {
      break;
    }
  }
  //
  // Disable the cache on the memory region assigned to QSPI.
  //
#ifndef CORE_M4
  _DisableDCache();
#endif
  //
  // NCS of bank 1 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << (NOR_BK1_NCS_BIT << 1));
  GPIOG_MODER   |=   MODER_ALT    << (NOR_BK1_NCS_BIT << 1);
  GPIOG_AFRL    &= ~(AFR_MASK     << (NOR_BK1_NCS_BIT << 2));
  GPIOG_AFRL    |=   0xAuL        << (NOR_BK1_NCS_BIT << 2);
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_NCS_BIT << 1));
  GPIOG_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_NCS_BIT << 1);
  //
  // NCS of bank 2 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOC_MODER   &= ~(MODER_MASK   << (NOR_BK2_NCS_BIT << 1));
  GPIOC_MODER   |=   MODER_ALT    << (NOR_BK2_NCS_BIT << 1);
  GPIOC_AFRH    &= ~(AFR_MASK     << ((NOR_BK2_NCS_BIT - 8) << 2));
  GPIOC_AFRH    |=   0xAuL        << ((NOR_BK2_NCS_BIT - 8) << 2);
  GPIOC_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK2_NCS_BIT << 1));
  GPIOC_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK2_NCS_BIT << 1);
  //
  // CLK is an output signal controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_CLK_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_CLK_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_CLK_BIT - 8) << 2));
  GPIOF_AFRH    |=   0x9uL        << ((NOR_CLK_BIT - 8) << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_CLK_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_CLK_BIT << 1);
  //
  // D0 of bank 1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOD_MODER   &= ~(MODER_MASK   << (NOR_BK1_D0_BIT << 1));
  GPIOD_MODER   |=   MODER_ALT    << (NOR_BK1_D0_BIT << 1);
  GPIOD_AFRH    &= ~(AFR_MASK     << ((NOR_BK1_D0_BIT - 8) << 2));
  GPIOD_AFRH    |=   0x9uL        << ((NOR_BK1_D0_BIT - 8) << 2);
  GPIOD_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D0_BIT << 1));
  GPIOD_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D0_BIT << 1);
  //
  // D1 of bank 1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D1_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_BK1_D1_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_BK1_D1_BIT - 8) << 2));
  GPIOF_AFRH    |=   0xAuL        << ((NOR_BK1_D1_BIT - 8) << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D1_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D1_BIT << 1);
  //
  // D2 of bank 1 is used as write protection that has to be disabled when not used for data exchange.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D2_BIT << 1));
  GPIOF_MODER   |=   MODER_OUTPUT << (NOR_BK1_D2_BIT << 1);
  GPIOF_BSRR     =   1uL          <<  NOR_BK1_D2_BIT;
  //
  // D3 of bank 1 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D3_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_BK1_D3_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D3_BIT << 2));
  GPIOF_AFRL    |=   0x9uL        << (NOR_BK1_D3_BIT << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D3_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D3_BIT << 1);
  //
  // D0 of bank 2 is an input/output signal and is controlled by the QUADSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << (NOR_BK2_D0_BIT << 1));
  GPIOH_MODER   |=   MODER_ALT    << (NOR_BK2_D0_BIT << 1);
  GPIOH_AFRL    &= ~(AFR_MASK     << (NOR_BK2_D0_BIT << 2));
  GPIOH_AFRL    |=   0x9uL        << (NOR_BK2_D0_BIT << 2);
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK2_D0_BIT << 1));
  GPIOH_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK2_D0_BIT << 1);
  //
  // D1 of bank 2 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << (NOR_BK2_D1_BIT << 1));
  GPIOH_MODER   |=   MODER_ALT    << (NOR_BK2_D1_BIT << 1);
  GPIOH_AFRL    &= ~(AFR_MASK     << (NOR_BK2_D1_BIT << 2));
  GPIOH_AFRL    |=   0x9uL        << (NOR_BK2_D1_BIT << 2);
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK2_D1_BIT << 1));
  GPIOH_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK2_D1_BIT << 1);
  //
  // D2 of bank 2 is used as write protection that has to be disabled when not used for data exchange.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << (NOR_BK2_D2_BIT << 1));
  GPIOG_MODER   |=   MODER_OUTPUT << (NOR_BK2_D2_BIT << 1);
  GPIOG_BSRR     =   1uL          <<  NOR_BK2_D2_BIT;
  //
  // D3 of bank 3 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << (NOR_BK2_D3_BIT << 1));
  GPIOG_MODER   |=   MODER_ALT    << (NOR_BK2_D3_BIT << 1);
  GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_BK2_D3_BIT - 8) << 2));
  GPIOG_AFRH    |=   0x9uL        << ((NOR_BK2_D3_BIT - 8) << 2);
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK2_D3_BIT << 1));
  GPIOG_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK2_D3_BIT << 1);
  //
  // Initialize the Quad-SPI controller.
  //
  Freq_Hz = FS_NOR_HW_FLASH_CLK_HZ;
  Div = (U32)_CalcClockDivider(&Freq_Hz);
  QUADSPI_CR  = 0
              | (1uL << CR_EN_BIT)                  // Enable the Quad-SPI unit.
              | (Div << CR_PRESCALER_BIT)
#if FS_NOR_HW_USE_DUAL_FLASH_MODE
              | (1uL << CR_DFM_BIT)
#endif // FS_NOR_HW_USE_DUAL_FLASH_MODE
              ;
  QUADSPI_DCR = 0
              | (1uL           << DCR_CKMODE_BIT)   // CLK signals stays HIGH when the NOR flash is not selected.
              | (DCR_FSIZE_MAX << DCR_FSIZE_BIT)    // We set the NOR flash size to maximum since this value is not known at this stage.
              ;
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
  _ConfigWPIfRequired(BusWidth);
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
  _ConfigWPIfRequired(BusWidth);
  _ClearFlags();
  if (NumBytesData) {
    QUADSPI_DLR = NumBytesData - 1;         // 0 means "read 1 byte".
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
  _ConfigWPIfRequired(BusWidth);
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
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32H745_ST_STM32H745_DISCO = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
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
