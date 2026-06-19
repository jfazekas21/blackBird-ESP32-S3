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

File      : FS_NOR_HW_SPIFI_STM32H753_ST_STM32H753I_EVAL.c
Purpose   : Low-level flash driver for STM32H7 QSPI interface.
Literature:
  [1] RM0433 Reference manual STM32H7x3 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H7x3_RM0433_Rev3.pdf)
  [2] STM32H753xI Errata sheet STM32H753xI rev Y device limitations
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H753_Errata_Rev2.pdf)
  [3] UM2199 User manual Evaluation board with STM32H753XI MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\EvalBoard\STM32H7xxI-EVAL1_UM-RevB.pdf)
  [4] Micron Serial NOR Flash Memory 3V, Twin-Quad I/O, 4KB, 32KB, 64KB, Sector Erase MT25TL01GBBB, MT25TL01GHBB
    (\\fileserver\Techinfo\Company\Micron\SerialNORFlash\MT25T_QLKT_L_01G_xBB_0.pdf)

Additional information:
  Pin assignment of the QSPI interface

  Name          Port pin    External connector
  --------------------------------------------
  QSPI_CLK      PB2         CN6.24
  QSPI_BK1_NCS  PG6         CN6.57
  QSPI_BK2_NCS  PC11        CN7.40
  QSPI_BK1_IO0  PF8         CN6.11
  QSPI_BK1_IO1  PF9         CN6.23
  QSPI_BK1_IO2  PF7         CN6.4
  QSPI_BK1_IO3  PF6         CN6.8
  QSPI_BK2_IO0  PH2         CN6.17
  QSPI_BK2_IO1  PH3         CN6.15
  QSPI_BK2_IO2  PG9         CN6.32
  QSPI_BK2_IO3  PG14        CN6.13

  The file system has to be configured with FS_NOR_LINE_SIZE set to 2
  in dual flash mode (FS_NOR_HW_USE_DUAL_FLASH_MODE set to 1)
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include <stddef.h>
#include "FS.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_NOR_HW_QUADSPI_CLK_HZ
  #define FS_NOR_HW_QUADSPI_CLK_HZ            200000000     // Clock of Quad-SPI unit
#endif

#ifndef   FS_NOR_HW_FLASH_CLK_HZ
  #define FS_NOR_HW_FLASH_CLK_HZ              50000000      // Frequency of the clock supplied to NOR flash device
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

#ifndef   FS_NOR_HW_USE_MEM_MAP_MODE
  #define FS_NOR_HW_USE_MEM_MAP_MODE          1             // Enables or disables the reading of NOR flash data via system memory.
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
#define NOR_CLK_BIT             2   // Port B
#define NOR_BK1_D0_BIT          8   // Port F
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
#define CR_SSHIFT_BIT           4
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
#define CYCLES_PER_MS           150000            // Depends on the CPU clock frequency.
#define WAIT_TIMEOUT_MS         500
#define WAIT_TIMEOUT_CYCLES     (WAIT_TIMEOUT_MS * CYCLES_PER_MS)

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CalcClockDivider
*
*  Function description
*    Calculates the divider for the clock supplied to NOR flash device.
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
*
*  Function description
*    Returns the transfer mode for the QUADSPI peripheral.
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
*       _CalcNumCycles
*
*  Function description
*    Calculates the number of clock cycles required to transfer a specified number of bytes.
*/
static U32 _CalcNumCycles(unsigned BusWidth, U32 NumBytes) {
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
*
*  Function description
*    Clears the status flags pf QUADSPI peripheral.
*/
static int _ClearFlags(void) {
  U32 TimeOut;
  int r;

  QUADSPI_FCR = 0
              | (1uL << SR_TEF_BIT)
              | (1uL << SR_TCF_BIT)
              | (1uL << SR_SMF_BIT)
              | (1uL << SR_TOF_BIT)
              ;
  //
  // Wait for the flags to be cleared.
  //
  r       = 0;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & ((1uL << SR_TEF_BIT) |
                       (1uL << SR_TCF_BIT) |
                       (1uL << SR_SMF_BIT) |
                       (1uL << SR_TOF_BIT))) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                    // Error, could not clear flags.
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*       _Cancel
*
*  Function description
*    Interrupts an ongoing data transfer.
*/
static int _Cancel(void) {
  U32 TimeOut;
  int r;

  r       = 0;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  QUADSPI_CR |= 1uL << CR_ABORT_BIT;
  while (1) {
    if ((QUADSPI_CR & (1uL << CR_ABORT_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                    // Error, could not cancel data transfer.
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*       _DisableDCache
*
*  Function description
*    Disables the data cache for the memory region where the NOR flash device is mapped.
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

#if FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _Map
*
*  Function description
*    Configures the hardware for access to serial NOR flash via system memory.
*/
static int _Map(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U16 BusWidth, unsigned Flags) {
  U32 CfgReg;
  U32 CmdMode;
  U32 AddrMode;
  U32 DataMode;
  U32 NumCyclesDummy;
  U32 AddrSize;
  U32 AltMode;
  U32 AltSize;
  U32 NumBytesAlt;
  U32 NumBytes;
  U32 AltReg;
  int r;
  U32 TimeOut;

  FS_USE_PARA(Unit);                                                                // This device has only one HW unit.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);                              // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Encode the mode and dummy bytes.
  //
  NumCyclesDummy = 0;
  if (NumBytesAlt != 0) {
    NumBytes = NumBytesAlt;
    //
    // Check if a mode byte has to be sent and if yes encode its value.
    //
    if (   ((Flags & FS_NOR_HW_FLAG_MODE_8BIT) != 0u)
        || ((Flags & FS_NOR_HW_FLAG_MODE_4BIT) != 0u)) {                            // QUADSPI is not able to send a number of mode bits that is not a multiple of eight.
      NumBytes    = NumBytesAlt - 1u;
      NumBytesAlt = 1;                                                              // We never send more than one mode byte.
      if (pPara != NULL) {
        AltReg = (U32)*pPara;
      } else {
        AltReg = 0xFFuL;
      }
    } else {
      NumBytesAlt = 0;
    }
    if (NumBytes != 0u) {
      //
      // The remaining alternate bytes are not sent. Instead, we generate dummy cycles.
      //
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes);
    }
  }
  //
  // It seems that QUADSPI is driving the data lines one half of the clock
  // cycle longer than required. This can cause that a wrong value is
  // sampled immediately after a data line is switched from output to input.
  // For this reason we configure QUADSPI to sample the data later than default.
  //
  if ((NumBytesAlt != 0) && (NumCyclesDummy == 0) ) {
    QUADSPI_CR |= 1uL << CR_SSHIFT_BIT;
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  CmdMode  = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), 1);                           // We always read at least one byte.
  //
  // Calculate the number of bytes in each command phase.
  //
  AddrSize = 0;
  if (NumBytesAddr) {
    AddrSize = (NumBytesAddr - 1) & CCR_ADSIZE_MASK;
  }
  AltSize = 0;
  if (NumBytesAlt) {
    AltSize = (NumBytesAlt - 1) & CCR_ABSIZE_MASK;
  }
  //
  // Configure the operation.
  //
  CfgReg = 0
         | (CCR_FMODE_MMAP << CCR_FMODE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (CmdMode        << CCR_IMODE_BIT)
         | (*pCmd          << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                // Error, QUADSPI is not ready.
      goto Done;
    }
  }
  _ConfigWPIfRequired(BusWidth);
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                                              // Error, could not clear status flags.
  }
  QUADSPI_AR  = 0;
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
Done:
  return r;
}

#endif // FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _Control
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*/
static int _Control(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, U8 BusWidth, unsigned Flags) {
  U32 CfgReg;
  U32 CmdMode;
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                        // This device has only one HW unit.
  FS_USE_PARA(Flags);
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);      // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r       = 0;
  CmdMode = _GetMode(BusWidth, NumBytesCmd);
  CfgReg  = 0
          | (CCR_FMODE_WRITE << CCR_FMODE_BIT)
          | (CmdMode         << CCR_IMODE_BIT)
          | (*pCmd           << CCR_INTRUCTION_BIT)
          ;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                // Error, QUADSPI is not ready.
      goto Done;
    }
  }
  //
  // Execute the command.
  //
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                                              // Error, could not clear status flags.
  }
  QUADSPI_DLR = 0;
  QUADSPI_ABR = 0;
  QUADSPI_CCR = CfgReg;
  //
  // Wait until the command has been completed.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                // Error, QUADSPI is not ready.
      break;
    }
  }
Done:
  return r;
}

/*********************************************************************
*
*       _Read
*
*  Function description
*    Transfers data from NOR flash to MCU.
*/
static int _Read(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
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
  U32 NumCyclesDummy;
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                                                // This device has only one HW unit.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);                              // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
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
  // Encode the mode and dummy bytes.
  //
  NumCyclesDummy = 0;
  if (NumBytesAlt != 0) {
    NumBytes = NumBytesAlt;
    //
    // Check if a mode byte has to be sent and if yes encode its value.
    //
    if (   ((Flags & FS_NOR_HW_FLAG_MODE_8BIT) != 0u)
        || ((Flags & FS_NOR_HW_FLAG_MODE_4BIT) != 0u)) {                           // QUADSPI is not able to send a number of mode bits that is not a multiple of eight.
      NumBytes    = NumBytesAlt - 1u;
      NumBytesAlt = 1;                                                              // We never send more than one mode byte.
      if (pPara != NULL) {
        AltReg = (U32)*pPara;
      } else {
        AltReg = 0xFFuL;
      }
    } else {
      NumBytesAlt = 0;
    }
    if (NumBytes != 0u) {
      //
      // The remaining alternate bytes are not sent. Instead, we generate dummy cycles.
      //
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes);
    }
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  CmdMode     = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode    = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode     = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode    = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
  //
  // Calculate the number of bytes in each command phase.
  //
  AddrSize = 0;
  if (NumBytesAddr) {
    //
    // Set the correct number of bytes to be sent to the NOR flash device.
    //
    if (NumBytesAddr == 4) {
      if ((Flags & FS_NOR_HW_FLAG_ADDR_3BYTE) != 0) {
        NumBytesAddr = 3;
      }
    }
    AddrSize = (NumBytesAddr - 1) & CCR_ADSIZE_MASK;
  }
  AltSize = 0;
  if (NumBytesAlt) {
    AltSize = (NumBytesAlt - 1) & CCR_ABSIZE_MASK;
  }
  //
  // Configure the operation.
  //
  CfgReg = 0
         | (CCR_FMODE_READ << CCR_FMODE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (CmdMode        << CCR_IMODE_BIT)
         | (*pCmd          << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, QUADSPI is not ready.
      goto Done;
    }
  }
  //
  // It seems that QUADSPI is driving the data lines one half of the clock
  // cycle longer than required. This can cause that a wrong value is
  // sampled immediately after a data line is switched from output to input.
  // For this reason we configure QUADSPI to sample the data later than default.
  //
  if ((NumBytesAlt != 0) && (NumCyclesDummy == 0) ) {
    QUADSPI_CR |= 1uL << CR_SSHIFT_BIT;
  }
  //
  // Execute the command.
  //
  _ConfigWPIfRequired(BusWidth);
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                            // Error, could not clear status flags.
  }
  if (NumBytesData) {
    QUADSPI_DLR = NumBytesData - 1;         // 0 means "read 1 byte".
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr != 0) {
    QUADSPI_AR = AddrReg;
  }
  //
  // Read data from NOR flash.
  //
  if (NumBytesData != 0) {
    do {
      //
      // Wait for the data to be received.
      //
      TimeOut = WAIT_TIMEOUT_CYCLES;
      for (;;) {
        NumBytesAvail = (QUADSPI_SR >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
        if ((NumBytesAvail >= 4) || (NumBytesAvail >= NumBytesData)) {
          break;
        }
        if (--TimeOut == 0u) {
          r = 1;                          // Error, no data received.
          goto Done;
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
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if (QUADSPI_SR & (1uL << SR_TCF_BIT)) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, transaction not completed.
      break;
    }
  }
Done:
  //
  // Restore the default sampling mode.
  //
  QUADSPI_CR &= ~(1uL << CR_SSHIFT_BIT);
  return r;
}

/*********************************************************************
*
*       _Write
*
*  Function description
*    Transfers data from MCU to NOR flash.
*/
static int _Write(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
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
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                        // This device has only one HW unit.
  FS_USE_PARA(Flags);
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);      // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
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
  // Encode additional information.
  //
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesAlt;
    do {
      AltReg <<= 8;
      AltReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  CmdMode     = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode    = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode     = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode    = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
  //
  // Calculate the number of bytes in each command phase.
  //
  AddrSize = 0;
  if (NumBytesAddr != 0) {
    //
    // Set the correct number of bytes to be sent to the NOR flash device.
    //
    if (NumBytesAddr == 4) {
      if ((Flags & FS_NOR_HW_FLAG_ADDR_3BYTE) != 0) {
        NumBytesAddr = 3;
      }
    }
    AddrSize = NumBytesAddr - 1;
  }
  AltSize = 0;
  if (NumBytesAlt != 0) {
    AltSize = NumBytesAlt - 1;
  }
  //
  // Configure the operation.
  //
  CfgReg = 0
         | (CCR_FMODE_WRITE << CCR_FMODE_BIT)
         | (DataMode        << CCR_DMODE_BIT)
         | (AltSize         << CCR_ABSIZE_BIT)
         | (AltMode         << CCR_ABMODE_BIT)
         | (AddrSize        << CCR_ADSIZE_BIT)
         | (AddrMode        << CCR_ADMODE_BIT)
         | (CmdMode         << CCR_IMODE_BIT)
         | (*pCmd           << CCR_INTRUCTION_BIT)
         ;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QUADSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, QUADSPI is not ready.
      goto Done;
    }
  }
  //
  // Execute the command.
  //
  _ConfigWPIfRequired(BusWidth);
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                            // Error, could not clear status flags.
  }
  if (NumBytesData != 0) {
    QUADSPI_DLR = NumBytesData - 1;       // 0 means "read 1 byte".
  }
  QUADSPI_ABR = AltReg;
  QUADSPI_CCR = CfgReg;
  if (NumBytesAddr != 0) {
    QUADSPI_AR = AddrReg;
  }
  //
  // write data to NOR flash.
  //
  if (NumBytesData != 0) {
    do {
      //
      // Wait for free space in FIFO.
      //
      TimeOut = WAIT_TIMEOUT_CYCLES;
      for (;;) {
        NumBytesFree = (QUADSPI_SR >> SR_FLEVEL_BIT) & SR_FLEVEL_MASK;
        NumBytesFree = NUM_BYTES_FIFO - NumBytesFree;
        if ((NumBytesFree >= 4) || (NumBytesFree >= NumBytesData)) {
          break;
        }
        if (--TimeOut == 0u) {
          r = 1;                          // Error, no free space in FIFO.
          goto Done;
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
    } while (NumBytesData != 0);
  }
  //
  // Wait until the data transfer has been completed.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if (QUADSPI_SR & (1uL << SR_TCF_BIT)) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, transfer not completed.
      break;
    }
  }
Done:
  return r;
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
  _DisableDCache();
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
  GPIOB_MODER   &= ~(MODER_MASK   << (NOR_CLK_BIT << 1));
  GPIOB_MODER   |=   MODER_ALT    << (NOR_CLK_BIT << 1);
  GPIOB_AFRL    &= ~(AFR_MASK     << (NOR_CLK_BIT << 2));
  GPIOB_AFRL    |=   0x9uL        << (NOR_CLK_BIT << 2);
  GPIOB_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_CLK_BIT << 1));
  GPIOB_OSPEEDR |=   OSPEEDR_HIGH << (NOR_CLK_BIT << 1);
  //
  // D0 of bank 1 is an input/output signal controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D0_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_BK1_D0_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_BK1_D0_BIT - 8) << 2));
  GPIOF_AFRH    |=   0xAuL        << ((NOR_BK1_D0_BIT - 8) << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D0_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D0_BIT << 1);
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
*       _HW_Unmap
*
*  Function description
*    Enables the direct access to NOR flash device.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*
*  Additional information
*    This function disables the memory-mapped mode.
*/
static void _HW_Unmap(U8 Unit) {
  FS_USE_PARA(Unit);            // This device has only one HW unit.
  //
  // Restore the default sampling mode.
  //
  QUADSPI_CR &= ~(1uL << CR_SSHIFT_BIT);
  //
  // Cancel the memory mode so that the BUSY bit goes to 0
  // and we can write to QUADSPI_CCR register.
  //
  if ((QUADSPI_CCR & (CCR_FMODE_MASK << CCR_FMODE_BIT)) == (CCR_FMODE_MMAP << CCR_FMODE_BIT)) {
    (void)_Cancel();
  }
}

#if FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _HW_Map
*
*  Function description
*    Configures the hardware for access to serial NOR flash via system memory.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    ReadCmd        Code of the command to be used for reading the data.
*    NumBytesAddr   Number of address bytes to be used for the read command.
*    NumBytesDummy  Number of dummy bytes to be sent after the read command.
*    BusWidth       Number of data lines to be used for the data transfer.
*/
static void _HW_Map(U8 Unit, U8 ReadCmd, unsigned NumBytesAddr, unsigned NumBytesDummy, U16 BusWidth) {
  unsigned NumBytesPara;

  NumBytesPara = NumBytesAddr + NumBytesDummy;
  (void)_Map(Unit, &ReadCmd, sizeof(ReadCmd), NULL, NumBytesPara, NumBytesAddr, BusWidth, 0);
}

#endif // FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _HW_Control
*
*  Function description
*    HW layer function. It requests the NOR flash to execute a simple command.
*    The HW has to be in SPI mode.
*/
static void _HW_Control(U8 Unit, U8 Cmd, U8 BusWidth) {
  (void)_Control(Unit, &Cmd, sizeof(Cmd), BusWidth, 0);
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    HW layer function. It transfers data from NOR flash to MCU.
*    The HW has to be in SPI mode.
*/
static void _HW_Read(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  (void)_Read(Unit, &Cmd, sizeof(Cmd), pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth, 0);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    HW layer function. It transfers data from MCU to NOR flash.
*    The HW has to be in SPI mode.
*/
static void _HW_Write(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  (void)_Write(Unit, &Cmd, sizeof(Cmd), pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth, 0);
}

#if FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _HW_MapEx
*
*  Function description
*    Configures the hardware for access to serial NOR flash via system memory.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be used for reading the data. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    BusWidth       Number of data lines to be used for the data transfer.
*    Flags          Options for the data exchange.
*
*  Return value
*    ==0      OK, memory mode configured successfully.
*    !=0      An error occurred.
*/
static int _HW_MapEx(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U16 BusWidth, unsigned Flags) {
  int r;

  r = _Map(Unit, pCmd, NumBytesCmd, pPara, NumBytesPara, NumBytesAddr, BusWidth, Flags);
  return r;
}

#endif // FS_NOR_HW_USE_MEM_MAP_MODE

/*********************************************************************
*
*       _HW_ControlEx
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be sent. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    BusWidth       Number of data lines to be used for sending the command.
*    Flags          Options for the data exchange.
*
*  Return value
*    ==0      OK, command transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_ControlEx(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, U8 BusWidth, unsigned Flags) {
  int r;

  r = _Control(Unit, pCmd, NumBytesCmd, BusWidth, Flags);
  return r;
}

/*********************************************************************
*
*       _HW_ReadEx
*
*  Function description
*    Transfers data from serial NOR flash to MCU.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be sent. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    pData          [OUT] Data read from the serial NOR flash device. Can be NULL.
*    NumBytesData   Number of bytes to read from the serial NOR flash device. Can be 0.
*    BusWidth       Number of data lines to be used for the data transfer.
*    Flags          Options for the data exchange.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_ReadEx(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
  int r;

  r = _Read(Unit, pCmd, NumBytesCmd, pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth, Flags);
  return r;
}

/*********************************************************************
*
*       _HW_WriteEx
*
*  Function description
*    Transfers data from MCU to serial NOR flash.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be sent. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    pData          [IN] Data to be sent to the serial NOR flash device. Can be NULL.
*    NumBytesData   Number of bytes to be written the serial NOR flash device. Can be 0.
*    BusWidth       Number of data lines to be used for the data transfer.
*    Flags          Options for the data exchange.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_WriteEx(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
  int r;

  r = _Write(Unit, pCmd, NumBytesCmd, pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth, Flags);
  return r;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32H753_ST_STM32H753I_EVAL = {
  _HW_Init,
  _HW_Unmap,
#if FS_NOR_HW_USE_MEM_MAP_MODE
  _HW_Map,
#else
  NULL,
#endif
  _HW_Control,
  _HW_Read,
  _HW_Write,
  NULL,
  NULL,
  NULL,
  NULL
#if (FS_VERSION >= 51800u)
#if FS_NOR_HW_USE_MEM_MAP_MODE
  , _HW_MapEx
#else
  , NULL
#endif
  , _HW_ControlEx
  , _HW_ReadEx
  , _HW_WriteEx
  , NULL
#endif
};

/*************************** End of file ****************************/
