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

File      : FS_NOR_HW_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator.c
Purpose   : Low-level flash driver for STM32H7 QSPI interface.
Literature:
  [1] RM0433 Reference manual STM32H7x3 advanced ARM-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H7x3_RM0433_Rev3.pdf)
  [2] STM32H753xI Errata sheet STM32H753xI rev Y device limitations
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32H7\STM32H753_Errata_Rev2.pdf)
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
  #define FS_NOR_HW_PER_CLK_HZ              240000000     // Clock of Quad-SPI unit
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ              48000000      // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_QUADSPI_MEM_ADDR
  #define FS_NOR_HW_QUADSPI_MEM_ADDR        0x90000000    // This is the start address of the memory region used by the file system
                                                          // to read the data from the serial NOR flash device. The hardware layer
                                                          // uses this address to invalidate the data cache. It should be set to the value passed
                                                          // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS                  0             // Enables/disables the interrupt-driven operation.
                                                          // Permitted values:
                                                          //   0 - polling via CPU
                                                          //   1 - event-driven using embOS
                                                          //   2 - event-driven using other RTOS
#endif

#ifndef   FS_NOR_HW_USE_QUAD_MODE
  #define FS_NOR_HW_USE_QUAD_MODE           1             // Enables/disables the support for quad mode.
#endif

#ifndef   FS_NOR_HW_USE_MEM_MAP_MODE
  #define FS_NOR_HW_USE_MEM_MAP_MODE        1             // Enables or disables the reading of NOR flash data via system memory.
#endif

#ifndef   FS_NOR_HW_RESET_TIME_MS
  #define FS_NOR_HW_RESET_TIME_MS           10            // Number milliseconds to keep the reset line low.
#endif

#ifndef     FS_NOR_HW_USE_DTR
  #if (FS_VERSION >= 51800u)
    #define FS_NOR_HW_USE_DTR               1             // Enables/disables the support for DTR mode.
  #else
    #define FS_NOR_HW_USE_DTR               0
  #endif
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "stm32h7xx.h"
  #include "FS_OS.h"
#if (FS_NOR_HW_USE_OS == 1)
  #include "RTOS.h"
#endif // FS_NOR_HW_USE_OS == 1
#endif // FS_NOR_HW_USE_OS

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
*       Port F registers
*/
#define GPIOF_BASE_ADDR         0x58021400uL
#define GPIOF_MODER             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x00))
#define GPIOF_OSPEEDR           (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x08))
#define GPIOF_PUPDR             (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x0C))
#define GPIOF_ODR               (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x14))
#define GPIOF_BSRR              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x18))
#define GPIOF_AFRL              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x20))
#define GPIOF_AFRH              (*(volatile U32 *)(GPIOF_BASE_ADDR + 0x24))

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
#define AHB1ENR_GPIOFEN         5
#define AHB3ENR_QSPIEN          14

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
#define NOR_RST_BIT             11  // Port F

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
*       Quad-SPI control register
*/
#define CR_EN_BIT               0
#define CR_ABORT_BIT            1
#define CR_SSHIFT_BIT           4
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
#define CCR_DDRM_BIT            31

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
#define QUADSPI_MEM_SIZE_SHIFT  28                // Size of the memory region reserved for QUADSPI (256 MB)
#define QUADSPI_PRIO            15
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
static U32 _CalcNumCycles(unsigned BusWidth, U32 NumBytes, int IsDTRMode) {
  U32 NumCycles;

  NumCycles = 0;
  if (NumBytes != 0) {
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
    if (IsDTRMode != 0) {
      NumCycles >>= 1;                // The data is sent on both clock edges.
    }
  }
  return NumCycles;
}

/*********************************************************************
*
*       _CalcNumCyclesEx
*
*  Function description
*    Calculates the number of clock cycles required to transfer a specified number of half-bytes.
*/
static U32 _CalcNumCyclesEx(unsigned BusWidth, U32 NumNibbles, int IsDTRMode) {
  U32 NumCycles;

  NumCycles = 0;
  if (NumNibbles != 0) {
    NumCycles = NumNibbles << 2;      // Assume 4-bits per nibble.
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
    if (IsDTRMode != 0) {
      NumCycles >>= 1;                // The data is sent on both clock edges.
    }
  }
  return NumCycles;
}

/*********************************************************************
*
*       _ClearFlags
*
*  Function description
*    Clears the status flags of QUADSPI peripheral.
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
  for (;;) {
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
  U32 IsDTR;
  U32 AltMode;
  U32 AltSize;
  U32 NumBytesAlt;
  U32 NumBytes;
  U32 AltReg;
  int r;
  U32 TimeOut;

  FS_USE_PARA(Unit);                                                                // This MCU has only one QSPI controller.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);                              // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Calculate the transfer mode.
  //
  IsDTR = 0;
  if (   ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u)
      || ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u)) {
    IsDTR = 1;
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, IsDTR);
    }
  }
  //
  // Check if any additional dummy cycles have to be generated.
  //
  if ((Flags & FS_NOR_HW_FLAG_DUMMY_4BIT) != 0u) {
    NumCyclesDummy += _CalcNumCyclesEx(FS_BUSWIDTH_GET_ADDR(BusWidth), 1, IsDTR);   // 1 dummy nibble.
  }
  //
  // It seems that QUADSPI is driving the data lines one half of the clock
  // cycle longer than required. This can cause that a wrong value is
  // sampled immediately after a data line is switched from output to input.
  // For this reason we configure QUADSPI to sample the data later than default.
  //
  if ((IsDTR == 0) && (NumBytesAlt != 0) && (NumCyclesDummy == 0) ) {
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
         | (AddrMode       << CCR_ADMODE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (IsDTR          << CCR_DDRM_BIT)
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
      r = 1;                                                                        // Error, QUADSPI is not ready.
      goto Done;
    }
  }
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                                                                      // Error, could not clear status flags.
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

  FS_USE_PARA(Unit);                                        // This MCU has only one QSPI controller.
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
  U32 IsDTR;
  U32 NumCyclesDummy;
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                                                // This MCU has only one QSPI controller.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);                              // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Calculate the transfer mode.
  //
  IsDTR = 0;
  if (   ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u)
      || ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u)) {
    IsDTR = 1;
  }
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, IsDTR);
    }
  }
  //
  // Check if any additional dummy cycles have to be generated.
  //
  if ((Flags & FS_NOR_HW_FLAG_DUMMY_4BIT) != 0u) {
    NumCyclesDummy += _CalcNumCyclesEx(FS_BUSWIDTH_GET_ADDR(BusWidth), 1, IsDTR);   // 1 dummy nibble.
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  CmdMode  = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
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
         | (CCR_FMODE_READ << CCR_FMODE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (IsDTR          << CCR_DDRM_BIT)
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
  if ((IsDTR == 0) && (NumBytesAlt != 0) && (NumCyclesDummy == 0) ) {
    QUADSPI_CR |= 1uL << CR_SSHIFT_BIT;
  }
  //
  // Execute the command.
  //
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
        } while (--NumBytesData != 0);
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
  U32 IsDTR;
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                        // This MCU has only one QSPI controller.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);      // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r           = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Calculate the transfer mode.
  //
  IsDTR = 0;
  if (   ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u)
      || ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u)) {
    IsDTR = 1;
  }
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
  if (NumBytesAddr) {
    AddrSize = NumBytesAddr - 1;
  }
  AltSize = 0;
  if (NumBytesAlt) {
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
         | (IsDTR           << CCR_DDRM_BIT)
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
        } while (--NumBytesData != 0);
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

#if FS_NOR_HW_USE_OS

/*********************************************************************
*
*       _Poll
*
*  Function description
*    Checks periodically the value of a status flag.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be sent. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    BitPos         Position of the bit to be checked.
*    BitValue       Value of the bit to wait for.
*    Delay          Number of clock cycles to wait between two queries.
*    TimeOut_ms     Maximum number of milliseconds to wait for the bit to be set.
*    BusWidth       Number of data lines to be used for the data transfer.
*    Flags          Options for the data exchange.
*
*  Return value
*    > 0    Timeout occurred.
*    ==0    OK, bit set to specified value.
*    < 0    Feature not supported.
*/
static int _Poll(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 BitPos, U8 BitValue, U32 Delay, U32 TimeOut_ms, U16 BusWidth, unsigned Flags) {
  U32 CfgReg;
  U32 DataMode;
  U32 CmdMode;
  U32 NumBytes;
  U32 NumBytesData;
  U32 DataMask;
  U32 DataMatch;
  U32 AddrReg;
  U32 AltReg;
  U32 NumBytesAlt;
  U32 AddrMode;
  U32 AltMode;
  U32 AddrSize;
  U32 AltSize;
  U32 IsDTR;
  U32 NumCyclesDummy;
  U32 TimeOut;
  int r;

  FS_USE_PARA(Unit);                                        // This MCU has only one QSPI controller.
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytesCmd == 1u);      // The QSPI controller is able to handle only 1-byte commands.
  //
  // Fill local variables.
  //
  r              = 0;
  AddrReg        = 0;
  AltReg         = 0;
  DataMask       = 1uL           << BitPos;
  DataMatch      = (U32)BitValue << BitPos;
  Delay        <<= 4;           // 16 clock cycles are required to transfer 2 bytes.
  NumBytesData   = 1;           // The response consists of only one byte.
  NumBytesAlt    = NumBytesPara - NumBytesAddr;
  //
  // Calculate the transfer mode.
  //
  IsDTR = 0;
  if (   ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u)
      || ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u)) {
    IsDTR = 1;
  }
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, IsDTR);
    }
  }
  //
  // Check if any additional dummy cycles have to be generated.
  //
  if ((Flags & FS_NOR_HW_FLAG_DUMMY_4BIT) != 0u) {
    NumCyclesDummy += _CalcNumCyclesEx(FS_BUSWIDTH_GET_ADDR(BusWidth), 1, IsDTR);   // 1 dummy nibble.
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  CmdMode  = _GetMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _GetMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _GetMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
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
         | (CCR_FMODE_POLL << CCR_FMODE_BIT)
         | (AddrMode       << CCR_ADMODE_BIT)
         | (AddrSize       << CCR_ADSIZE_BIT)
         | (AltMode        << CCR_ABMODE_BIT)
         | (AltSize        << CCR_ABSIZE_BIT)
         | (DataMode       << CCR_DMODE_BIT)
         | (NumCyclesDummy << CCR_DCYC_BIT)
         | (IsDTR          << CCR_DDRM_BIT)
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
  // Start the poll operation. The end of operation is signaled by interrupt.
  //
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                            // Error, could not clear status flags.
  }
  if (NumBytes) {
    QUADSPI_DLR = NumBytes - 1;           // 0 means "read 1 byte".
  }
  QUADSPI_ABR   = AltReg;
  QUADSPI_PSMKR = DataMask;
  QUADSPI_PSMAR = DataMatch;
  QUADSPI_PIR   = Delay;
  QUADSPI_FCR   = 1uL << SR_SMF_BIT;
  QUADSPI_CCR   = CfgReg;
  if (NumBytesAddr != 0u) {
    QUADSPI_AR  = AddrReg;
  }
  r = FS_OS_Wait(TimeOut_ms);
  if (r != 0) {
    //
    // The timeout has expired. Cancel the poll operation.
    //
    (void)_Cancel();
    r = 1;                                // A timeout occurred.
  }
Done:
  return r;
}

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
               | (1uL << AHB1ENR_GPIOFEN)
               ;
  RCC_AHB3ENR  |=   1uL << AHB3ENR_QSPIEN;
  RCC_AHB3RSTR |=   1uL << AHB3ENR_QSPIEN;
  RCC_AHB3RSTR &= ~(1uL << AHB3ENR_QSPIEN);
  //
  // Wait for the unit to exit reset.
  //
  for (;;) {
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
  GPIOB_MODER   &= ~(MODER_MASK   << (NOR_BK1_NCS_BIT << 1));
  GPIOB_MODER   |=   MODER_ALT    << (NOR_BK1_NCS_BIT << 1);
  GPIOB_AFRL    &= ~(AFR_MASK     << (NOR_BK1_NCS_BIT << 2));
  GPIOB_AFRL    |=   0xAuL        << (NOR_BK1_NCS_BIT << 2);
  GPIOB_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_NCS_BIT << 1));
  GPIOB_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_NCS_BIT << 1);
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
#if FS_NOR_HW_USE_QUAD_MODE
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
  // D3 of bank 1 is an output signal and is controlled by the QUADSPI unit.
  //
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D3_BIT << 1));
  GPIOF_MODER   |=   MODER_ALT    << (NOR_BK1_D3_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D3_BIT << 2));
  GPIOF_AFRL    |=   0x9uL        << (NOR_BK1_D3_BIT << 2);
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D3_BIT << 1));
  GPIOF_OSPEEDR |=   OSPEEDR_HIGH << (NOR_BK1_D3_BIT << 1);
#else
  //
  // D2 and D3 are used as WP and HOLD signals when the quad mode
  // is disabled in the NOR flash device. We set these signals to
  // logic high here so that the data transfer works correctly
  // in single and dual mode.
  //
  GPIOF_BSRR    = 1uL << NOR_BK1_D2_BIT;
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D2_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_BK1_D2_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D2_BIT << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D2_BIT << 1));
  GPIOF_BSRR    = 1uL << NOR_BK1_D3_BIT;
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_BK1_D3_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_BK1_D3_BIT << 1);
  GPIOF_AFRL    &= ~(AFR_MASK     << (NOR_BK1_D3_BIT << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_BK1_D3_BIT << 1));
#endif // FS_NOR_HW_USE_QUAD_MODE
  //
  // The MP6 is the signal connected to the pin 6 of the NOR flash device
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
  //
  // Set the RST signal to high to make sure that the NOR flash
  // device is not accidentally reset during the data transfer.
  //
  GPIOF_BSRR    = 1uL << NOR_RST_BIT;
  GPIOF_MODER   &= ~(MODER_MASK   << (NOR_RST_BIT << 1));
  GPIOF_MODER   |=   MODER_OUT    << (NOR_RST_BIT << 1);
  GPIOF_AFRH    &= ~(AFR_MASK     << ((NOR_RST_BIT - 8) << 2));
  GPIOF_OSPEEDR &= ~(OSPEEDR_MASK << (NOR_RST_BIT << 1));
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
#if FS_NOR_HW_RESET_TIME_MS
  {
    volatile U32 NumCycles;

    GPIOF_BSRR = 1uL << (NOR_RST_BIT + 16);               // Put the NOR flash device into reset state by setting the signal to low.
    NumCycles = FS_NOR_HW_RESET_TIME_MS * CYCLES_PER_MS;
    do {
      ;
    } while (--NumCycles != 0u);
    GPIOF_BSRR = 1uL << NOR_RST_BIT;                      // Release the NOR flash devvice from the reset state by setting reset signal to high.
    NumCycles = FS_NOR_HW_RESET_TIME_MS * CYCLES_PER_MS;
    do {
      ;
    } while (--NumCycles != 0u);
  }
#endif // FS_NOR_HW_RESET_TIME_MS
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

/*********************************************************************
*
*       _HW_Poll
*
*  Function description
*    Checks periodically the value of a status flag.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    Cmd            Code of the command to be sent.
*    BitPos         Position of the bit to be checked.
*    BitValue       Value of the bit to wait for.
*    Delay          Number of clock cycles to wait between two queries.
*    TimeOut_ms     Maximum number of milliseconds to wait for the bit to be set.
*    BusWidth       Number of data lines to be used for the data transfer.
*
*  Return value
*    > 0    Timeout occurred.
*    ==0    OK, bit set to specified value.
*    < 0    Feature not supported.
*/
static int _HW_Poll(U8 Unit, U8 Cmd, U8 BitPos, U8 BitValue, U32 Delay, U32 TimeOut_ms, U16 BusWidth) {
  int r;

#if FS_NOR_HW_USE_OS
  r = _Poll(Unit, &Cmd, sizeof(Cmd), NULL, 0, 0, BitPos, BitValue, Delay, TimeOut_ms, BusWidth, 0);
#else
  FS_USE_PARA(Unit);
  FS_USE_PARA(Cmd);
  FS_USE_PARA(BitPos);
  FS_USE_PARA(BitValue);
  FS_USE_PARA(Delay);
  FS_USE_PARA(TimeOut_ms);
  FS_USE_PARA(BusWidth);
  r = -1;                 // Set to indicate that the feature is not supported.
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
    FS_OS_Delay(ms);
    r = 0;
  }
#else
  FS_USE_PARA(ms);
#endif  // FS_NOR_HW_USE_OS
  return r;
}

#if FS_NOR_HW_USE_DTR

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
*       _HW_PollEx
*
*  Function description
*    Checks periodically the value of a status flag.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    pCmd           [IN] Code of the command to be sent. Cannot be NULL.
*    NumBytesCmd    Number of bytes in the command code. Cannot be 0.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    BitPos         Position of the bit to be checked.
*    BitValue       Value of the bit to wait for.
*    Delay          Number of clock cycles to wait between two queries.
*    TimeOut_ms     Maximum number of milliseconds to wait for the bit to be set.
*    BusWidth       Number of data lines to be used for the data transfer.
*    Flags          Options for the data exchange.
*
*  Return value
*    > 0    Timeout occurred.
*    ==0    OK, bit set to specified value.
*    < 0    Feature not supported.
*/
static int _HW_PollEx(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 BitPos, U8 BitValue, U32 Delay, U32 TimeOut_ms, U16 BusWidth, unsigned Flags) {
  int r;

#if FS_NOR_HW_USE_OS
  r = _Poll(Unit, pCmd, NumBytesCmd, pPara, NumBytesPara, NumBytesAddr, BitPos, BitValue, Delay, TimeOut_ms, BusWidth, Flags);
#else
  FS_USE_PARA(Unit);
  FS_USE_PARA(pCmd);
  FS_USE_PARA(NumBytesCmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BitPos);
  FS_USE_PARA(BitValue);
  FS_USE_PARA(Delay);
  FS_USE_PARA(TimeOut_ms);
  FS_USE_PARA(BusWidth);
  FS_USE_PARA(Flags);
  r = -1;                 // Set to indicate that the feature is not supported.
#endif
  return r;
}

#endif // FS_NOR_HW_USE_DTR

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
  OS_EnterNestableInterrupt();
#endif // FS_NOR_HW_USE_OS == 1
  QUADSPI_FCR = 1uL << SR_SMF_BIT;    // Prevent other interrupts from occurring.
  FS_OS_Signal();
#if (FS_NOR_HW_USE_OS == 1)
  OS_LeaveNestableInterrupt();
#endif // FS_NOR_HW_USE_OS == 1
}

#endif // FS_NOR_HW_USE_OS

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_NOR_HW_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32H743_SEGGER_QSPI_Flash_Evaluator = {
  _HW_Init,
  _HW_Unmap,
#if FS_NOR_HW_USE_MEM_MAP_MODE
  _HW_Map,
#else
  NULL,
#endif // FS_NOR_HW_USE_MEM_MAP_MODE
  _HW_Control,
  _HW_Read,
  _HW_Write,
  _HW_Poll,
  _HW_Delay,
  NULL,
  NULL
#if FS_NOR_HW_USE_DTR
#if FS_NOR_HW_USE_MEM_MAP_MODE
  , _HW_MapEx
#else
  , NULL
#endif // FS_NOR_HW_USE_MEM_MAP_MODE
  , _HW_ControlEx
  , _HW_ReadEx
  , _HW_WriteEx
  , _HW_PollEx
#else
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
#endif // FS_NOR_HW_USE_DTR
};

/*************************** End of file ****************************/
