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

File      : FS_NOR_HW_SPIFI_STM32L4R9_ST_STM32L4R9I_DISCO.c
Purpose   : Low-level flash driver for STM32L4R9 OCTASPI interface.
Literature:
  [1] RM0432 Reference manual STM32L4Rxxx and STM32L4Sxxx advanced Arm-based 32-bit MCUs
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\RM0432_STM32L4Sxxx_L4Rxxx_Rev05.pdf)
  [2] Datasheet  STM32L4R5xx STM32L4R7xx STM32L4R9xx
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\DS12023_STM32L4R5xx_Rev4.pdf)
  [3] STM32L4Rxxx STM32L4Sxxx Errata sheet
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\ES0393_STM32L4R_Errata_rev3.pdf)
  [4] UM2271 User manual Discovery kit with STM32L4R9AI MCU
    (\\FILESERVER\Techinfo\Company\ST\MCU\STM32\STM32L4\Evalboard\STM32L4R9_DiscoveryKit\en.DM00421316.pdf)
  [5] Datasheet MX25LM51245G
    ("\\FILESERVER\Techinfo\Company\Macronix\SPI_NOR_Flash\MX25LM51245G, 3V, 512Mb, v1.1.pdf")
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
  #define FS_NOR_HW_NOR_CLK_HZ      30000000uL      // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS          0               // Enables/disables the interrupt-driven operation.
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
*       Octo-SPI registers
*/
#define OCTOSPI_BASE_ADDR       0xA0001400uL
#define OCTOSPI_CR              (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x000))
#define OCTOSPI_DCR1            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x008))
#define OCTOSPI_DCR2            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x00C))
#define OCTOSPI_DCR3            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x010))
#define OCTOSPI_SR              (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x020))
#define OCTOSPI_FCR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x024))
#define OCTOSPI_DLR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x040))
#define OCTOSPI_AR              (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x048))
#define OCTOSPI_DR              (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x050))
#define OCTOSPI_DR_BYTE         (*(volatile U8* )(OCTOSPI_BASE_ADDR + 0x050))
#define OCTOSPI_PSMKR           (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x080))
#define OCTOSPI_PSMAR           (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x088))
#define OCTOSPI_PIR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x090))
#define OCTOSPI_CCR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x100))
#define OCTOSPI_TCR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x108))
#define OCTOSPI_IR              (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x110))
#define OCTOSPI_ABR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x120))
#define OCTOSPI_LPTR            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x130))
#define OCTOSPI_WCCR            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x180))
#define OCTOSPI_WTCR            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x188))
#define OCTOSPI_WIR             (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x190))
#define OCTOSPI_WABR            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x1A0))
#define OCTOSPI_HLCR            (*(volatile U32*)(OCTOSPI_BASE_ADDR + 0x200))

/*********************************************************************
*
*       Port G registers
*/
#define GPIOG_BASE_ADDR         0x48001800uL
#define GPIOG_MODER             (*(volatile U32*)(GPIOG_BASE_ADDR + 0x00))
#define GPIOG_OSPEEDR           (*(volatile U32*)(GPIOG_BASE_ADDR + 0x08))
#define GPIOG_PUPDR             (*(volatile U32*)(GPIOG_BASE_ADDR + 0x0C))
#define GPIOG_ODR               (*(volatile U32*)(GPIOG_BASE_ADDR + 0x14))
#define GPIOG_AFRH              (*(volatile U32*)(GPIOG_BASE_ADDR + 0x24))
#define GPIOG_AFRL              (*(volatile U32*)(GPIOG_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Port H registers
*/
#define GPIOH_BASE_ADDR         0x48001C00uL
#define GPIOH_MODER             (*(volatile U32*)(GPIOH_BASE_ADDR + 0x00))
#define GPIOH_OSPEEDR           (*(volatile U32*)(GPIOH_BASE_ADDR + 0x08))
#define GPIOH_PUPDR             (*(volatile U32*)(GPIOH_BASE_ADDR + 0x0C))
#define GPIOH_ODR               (*(volatile U32*)(GPIOH_BASE_ADDR + 0x14))
#define GPIOH_AFRL              (*(volatile U32*)(GPIOH_BASE_ADDR + 0x20))
#define GPIOH_AFRH              (*(volatile U32*)(GPIOH_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Port I registers
*/
#define GPIOI_BASE_ADDR         0x48002000uL
#define GPIOI_MODER             (*(volatile U32*)(GPIOI_BASE_ADDR + 0x00))
#define GPIOI_OSPEEDR           (*(volatile U32*)(GPIOI_BASE_ADDR + 0x08))
#define GPIOI_PUPDR             (*(volatile U32*)(GPIOI_BASE_ADDR + 0x0C))
#define GPIOI_ODR               (*(volatile U32*)(GPIOI_BASE_ADDR + 0x14))
#define GPIOI_AFRL              (*(volatile U32*)(GPIOI_BASE_ADDR + 0x20))
#define GPIOI_AFRH              (*(volatile U32*)(GPIOI_BASE_ADDR + 0x24))

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR           0x40021000uL
#define RCC_CFGR                (*(volatile U32*)(RCC_BASE_ADDR + 0x08))
#define RCC_AHB3RSTR            (*(volatile U32*)(RCC_BASE_ADDR + 0x30))
#define RCC_AHB1ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x48))
#define RCC_AHB2ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x4C))
#define RCC_AHB3ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x50))

/*********************************************************************
*
*       Power control
*/
#define PWR_BASE_ADDR           0x40007000uL
#define PWR_CR2                 (*(volatile U32*)(PWR_BASE_ADDR + 0x04))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB2ENR_GPIOGEN         6
#define AHB2ENR_GPIOHEN         7
#define AHB2ENR_GPIOIEN         8
#define AHB2ENR_OSPIMEN         20
#define AHB3ENR_OSPI2EN         9

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_CS_BIT              12    // Port G
#define NOR_DQS_BIT             15    // Port G
#define NOR_CLK_BIT             6     // Port I
#define NOR_D0_BIT              11    // Port I
#define NOR_D1_BIT              10    // Port I
#define NOR_D2_BIT              9     // Port I
#define NOR_D3_BIT              8     // Port H
#define NOR_D4_BIT              9     // Port H
#define NOR_D5_BIT              10    // Port H
#define NOR_D6_BIT              9     // Port G
#define NOR_D7_BIT              10    // Port G

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
*       Octo-SPI control register
*/
#define CR_EN_BIT               0
#define CR_ABORT_BIT            1
#define CR_DMAEN_BIT            2
#define CR_TCEN_BIT             3
#define CR_FTHRES_BIT           8
#define CR_FTHRES_MASK          0x2Fu
#define CR_TRIE_BIT             16
#define CR_TCIE_BIT             17
#define CR_FTIE_BIT             18
#define CR_SMIE_BIT             19
#define CR_TOIE_BIT             20
#define CR_APMS_BIT             22
#define CR_FMODE_BIT            28
#define CR_FMODE_WRITE          0x00uL
#define CR_FMODE_READ           0x01uL
#define CR_FMODE_POLL           0x02uL
#define CR_FMODE_MMAP           0x03uL
#define CR_FMODE_MASK           0x03uL

/*********************************************************************
*
*       Octo-SPI device configuration register 1
*/
#define DCR1_CKMODE_BIT         0
#define DCR1_CSHT_BIT           8
#define DCR1_DEVSIZE_BIT        16
#define DCR1_DEVSIZE_MAX        0x1Fu

/*********************************************************************
*
*       Octo-SPI device configuration register 2
*/
#define DCR2_PRESCALER_BIT      0
#define DCR2_PRESCALER_MAX      0xFFu

/*********************************************************************
*
*       Octo-SPI status register
*/
#define SR_TEF_BIT              0
#define SR_TCF_BIT              1
#define SR_FTF_BIT              2
#define SR_SMF_BIT              3
#define SR_TOF_BIT              4
#define SR_BUSY_BIT             5
#define SR_FLEVEL_BIT           8
#define SR_FLEVEL_MASK          0x3Fu

/*********************************************************************
*
*       Octo-SPI communication configuration register
*/
#define CCR_IMODE_BIT           0
#define CCR_ADMODE_BIT          8
#define CCR_ADSIZE_BIT          12
#define CCR_ADSIZE_MASK         0x03uL
#define CCR_ABMODE_BIT          16
#define CCR_ABSIZE_BIT          20
#define CCR_ABSIZE_MASK         0x03uL
#define CCR_DMODE_BIT           24
#define CCR_DMODE_NONE          0x00uL
#define CCR_DMODE_SINGLE        0x01uL
#define CCR_DMODE_DUAL          0x02uL
#define CCR_DMODE_QUAD          0x03uL
#define CCR_DMODE_OCTO          0x04uL

/*********************************************************************
*
*       Octo-SPI timing configuration register
*/
#define TCR_DCYC_BIT            0
#define TCR_DCYC_MASK           0x0FuL
#define TCR_DHQC_BIT            28

/*********************************************************************
*
*       Misc. defines
*/
#define NUM_BYTES_FIFO          16
#define OCTOSPI_PRIO            15
#define WAIT_TIMEOUT_MS         1000
#define CYCLES_PER_MS           150           // Blocks the execution for about 1ms on a CPU running at 78 Mhz.
#define WAIT_TIMEOUT_CYCLES     (WAIT_TIMEOUT_MS * CYCLES_PER_MS)
#define CR2_IOSV_BIT            9
#define LOW_POWER_TIMEOUT       32

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
static            U32 _ClkDiv;
#if FS_NOR_HW_USE_OS
  static volatile U32 _StatusOSPI;
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
    if (Div == DCR2_PRESCALER_MAX) {
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

  Mode = CCR_DMODE_NONE;
  if (NumBytes) {
    switch (BusWidth) {
    case 1:
      Mode = CCR_DMODE_SINGLE;
      break;
    case 2:
      Mode = CCR_DMODE_DUAL;
      break;
    case 4:
      Mode = CCR_DMODE_QUAD;
      break;
    case 8:
      Mode = CCR_DMODE_OCTO;
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
*       _WaitForTransferComplete
*/
static int _WaitForTransferComplete(void) {
  int r;
  U32 Status;

  r = 0;              // Set to indicate success.
#if FS_NOR_HW_USE_OS
  for (;;) {
    Status = _StatusOSPI;
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
    Status = OCTOSPI_SR;
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
    Status = _StatusOSPI;
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
    Status = OCTOSPI_SR;
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
    Status = _StatusOSPI;
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
    Status = OCTOSPI_SR;
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
*       _ClearFlags
*/
static void _ClearFlags(void) {
  OCTOSPI_FCR = 0
              | (1uL << SR_TEF_BIT)
              | (1uL << SR_TCF_BIT)
              | (1uL << SR_SMF_BIT)
              | (1uL << SR_TOF_BIT)
              ;
  //
  // Wait for the flags to be cleared.
  //
  for (;;) {
    if ((OCTOSPI_SR & ((1uL << SR_TEF_BIT) |
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
  if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) != 0) {
    OCTOSPI_CR |= 1uL << CR_ABORT_BIT;
    _WaitForTransferComplete();
    OCTOSPI_FCR = 1uL << SR_TCF_BIT;
    for (;;) {
      if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
        break;
      }
    }
  }
}

/*********************************************************************
*
*       _Reset
*/
static void _Reset(void) {
  RCC_AHB3RSTR |=  (1uL << AHB3ENR_OSPI2EN);
  RCC_AHB3RSTR &= ~(1uL << AHB3ENR_OSPI2EN);
  OCTOSPI_CR   = 0
               | (1uL << CR_EN_BIT)                       // Enable the Octo-SPI unit.
               | (1uL << CR_TCEN_BIT)                     // Enable the timeout for the memory mapped read operations.
               ;
  OCTOSPI_DCR1 = 0                                        // Note 1
               | (DCR1_DEVSIZE_MAX << DCR1_DEVSIZE_BIT)   // We set the NOR flash size to maximum since this value is not known at this stage.
               | (2uL              << DCR1_CSHT_BIT)      // This was taken from an ST sample.
               ;
  OCTOSPI_DCR2 = 0
               | (_ClkDiv << DCR2_PRESCALER_BIT)          // Configure the clock frequency of the NOR flash.
               ;
  OCTOSPI_TCR  = 0
               | (1uL << TCR_DHQC_BIT)                    // This was taken from an ST sample.
               ;
  OCTOSPI_LPTR = LOW_POWER_TIMEOUT;
#if FS_NOR_HW_USE_OS
  //
  // Clear interrupt flags and enable the interrupt.
  //
  OCTOSPI_CR  |= 0
              | (1uL << CR_APMS_BIT)
              | (1uL << CR_TRIE_BIT)
              | (1uL << CR_TCIE_BIT)
              | (1uL << CR_SMIE_BIT)
              | (1uL << CR_TOIE_BIT)
              ;
  OCTOSPI_FCR  = 0
               | (1uL << SR_SMF_BIT)
               | (1uL << SR_TOF_BIT)
               | (1uL << SR_FTF_BIT)
               | (1uL << SR_TCF_BIT)
               | (1uL << SR_TEF_BIT)
               ;
  _StatusOSPI = 0;
#endif // FS_NOR_HW_USE_OS
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
        *pData++ = OCTOSPI_DR_BYTE;
      } while (--NumBytes);
    } else {
      //
      // Read 4 bytes at a time.
      //
      DataReg = OCTOSPI_DR;
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
        OCTOSPI_DR_BYTE = *pData++;
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
      OCTOSPI_DR = DataReg;
    }
  } while (NumBytes != 0u);
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
*
*  Notes
*    (1) We configure the data transfer in Mode 0 (CLK must stay low while nCS is high)
*        because of the errata "2.7.6  Write data lost with clock mode 3" in [3].
*/
static int _HW_Init(U8 Unit) {
  U32 Freq_Hz;

  FS_USE_PARA(Unit);
  //
  // Enable the clocks of peripherals and reset them.
  //
  RCC_AHB2ENR  |= 0
               | (1uL << AHB2ENR_GPIOGEN)
               | (1uL << AHB2ENR_GPIOHEN)
               | (1uL << AHB2ENR_GPIOIEN)
               | (1uL << AHB2ENR_OSPIMEN)
               ;
  RCC_AHB3ENR  |=  (1uL << AHB3ENR_OSPI2EN);
  //
  // Wait for the unit to exit reset.
  //
  for (;;) {
    if ((RCC_AHB3RSTR & (1uL << AHB3ENR_OSPI2EN)) == 0) {
      break;
    }
  }
  //
  // CS is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << ( NOR_CS_BIT       << 1));
  GPIOG_MODER   |=  (MODER_ALT    << ( NOR_CS_BIT       << 1));
  GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_CS_BIT - 8)  << 2));
  GPIOG_AFRH    |=  (0x5uL        << ((NOR_CS_BIT - 8)  << 2));
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_CS_BIT       << 1));
  GPIOG_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_CS_BIT       << 1));
  //
  // DQS is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << ( NOR_DQS_BIT      << 1));
  GPIOG_MODER   |=  (MODER_ALT    << ( NOR_DQS_BIT      << 1));
  GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_DQS_BIT - 8) << 2));
  GPIOG_AFRH    |=  (0x5uL        << ((NOR_DQS_BIT - 8) << 2));
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_DQS_BIT      << 1));
  GPIOG_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_DQS_BIT      << 1));
  //
  // CLK is an output signal controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_CLK_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_CLK_BIT      << 1));
  GPIOI_AFRL    &= ~(AFR_MASK     << ((NOR_CLK_BIT - 0) << 2));
  GPIOI_AFRL    |=  (0x5uL        << ((NOR_CLK_BIT - 0) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_CLK_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_CLK_BIT      << 1));
  //
  // D0 is an input/output signal controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D0_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D0_BIT      << 1));
  GPIOI_AFRH    &= ~(AFR_MASK     << ((NOR_D0_BIT - 8) << 2));
  GPIOI_AFRH    |=  (0x5uL        << ((NOR_D0_BIT - 8) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D0_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D0_BIT      << 1));
  //
  // D1 is an input/output signal controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D1_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D1_BIT      << 1));
  GPIOI_AFRH    &= ~(AFR_MASK     << ((NOR_D1_BIT - 8) << 2));
  GPIOI_AFRH    |=  (0x5uL        << ((NOR_D1_BIT - 8) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D1_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D1_BIT      << 1));
  //
  // D2 is an input/output signal and is controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D2_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D2_BIT      << 1));
  GPIOI_AFRH    &= ~(AFR_MASK     << ((NOR_D2_BIT - 8) << 2));
  GPIOI_AFRH    |=  (0x5uL        << ((NOR_D2_BIT - 8) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D2_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D2_BIT      << 1));
  //
  // D3 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_D3_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_D3_BIT      << 1));
  GPIOH_AFRH    &= ~(AFR_MASK     << ((NOR_D3_BIT - 8) << 2));
  GPIOH_AFRH    |=  (0x5uL        << ((NOR_D3_BIT - 8) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D3_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D3_BIT      << 1));
  //
  // D4 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_D4_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_D4_BIT      << 1));
  GPIOH_AFRH    &= ~(AFR_MASK     << ((NOR_D4_BIT - 8) << 2));
  GPIOH_AFRH    |=  (0x5uL        << ((NOR_D4_BIT - 8) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D4_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D4_BIT      << 1));
  //
  // D5 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_D5_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_D5_BIT      << 1));
  GPIOH_AFRH    &= ~(AFR_MASK     << ((NOR_D5_BIT - 8) << 2));
  GPIOH_AFRH    |=  (0x5uL        << ((NOR_D5_BIT - 8) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D5_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D5_BIT      << 1));
  //
  // D6 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << ( NOR_D6_BIT      << 1));
  GPIOG_MODER   |=  (MODER_ALT    << ( NOR_D6_BIT      << 1));
  GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_D6_BIT - 8) << 2));
  GPIOG_AFRH    |=  (0x5uL        << ((NOR_D6_BIT - 8) << 2));
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D6_BIT      << 1));
  GPIOG_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D6_BIT      << 1));
  //
  // D7 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOG_MODER   &= ~(MODER_MASK   << ( NOR_D7_BIT      << 1));
  GPIOG_MODER   |=  (MODER_ALT    << ( NOR_D7_BIT      << 1));
  GPIOG_AFRH    &= ~(AFR_MASK     << ((NOR_D7_BIT - 8) << 2));
  GPIOG_AFRH    |=  (0x5uL        << ((NOR_D7_BIT - 8) << 2));
  GPIOG_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D7_BIT      << 1));
  GPIOG_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D7_BIT      << 1));
  //
  // Enable the power of the GPIO G.
  //
  PWR_CR2 |= (1uL << CR2_IOSV_BIT);
#if FS_NOR_HW_USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(OCTOSPI2_IRQn);
  NVIC_SetPriority(OCTOSPI2_IRQn, OCTOSPI_PRIO);
#endif // FS_NOR_HW_USE_OS
  //
  // Initialize the Octo-SPI controller.
  //
  Freq_Hz = FS_NOR_HW_NOR_CLK_HZ;
  _ClkDiv = _CalcClockDivider(&Freq_Hz);
  _Reset();
#if FS_NOR_HW_USE_OS
  NVIC_EnableIRQ(OCTOSPI2_IRQn);
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
  // and we can write to OCTOSPI_CCR register.
  //
  if ((OCTOSPI_CR & (CR_FMODE_MASK << CR_FMODE_BIT)) == (CR_FMODE_MMAP << CR_FMODE_BIT)) {
    _Cancel();
  }
  _Reset();
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
  U32 CtrlReg;
  U32 CmdMode;
  U32 AddrMode;
  U32 DataMode;
  U32 NumCyclesDummy;
  U32 AddrSize;
  U32 TCfgReg;

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
         | (DataMode << CCR_DMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (CmdMode  << CCR_IMODE_BIT)
         ;
  CtrlReg  =   OCTOSPI_CR;
  CtrlReg &= ~(CR_FMODE_MASK << CR_FMODE_BIT);
  CtrlReg |=   CR_FMODE_MMAP << CR_FMODE_BIT;
  TCfgReg  =   OCTOSPI_TCR;
  TCfgReg &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  TCfgReg |=   NumCyclesDummy << TCR_DCYC_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  _ClearFlags();
  OCTOSPI_CR  = CtrlReg;
  OCTOSPI_TCR = TCfgReg;
  OCTOSPI_AR  = 0;
  OCTOSPI_CCR = CfgReg;
  OCTOSPI_IR  = ReadCmd;
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
  U32 CtrlReg;
  U32 CmdMode;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Fill local variables.
  //
  CmdMode = _GetMode(BusWidth, sizeof(Cmd));
  CfgReg  = 0
          | (CmdMode << CCR_IMODE_BIT)
          ;
  CtrlReg  = OCTOSPI_CR;
  CtrlReg &= ~(CR_FMODE_MASK  << CR_FMODE_BIT);
  CtrlReg |=   CR_FMODE_WRITE << CR_FMODE_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
  OCTOSPI_CR  = CtrlReg;
  OCTOSPI_DLR = 0;
  OCTOSPI_ABR = 0;
  OCTOSPI_CCR = CfgReg;
  OCTOSPI_IR  = Cmd;
  //
  // Wait until the command has been completed.
  //
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
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
  U32 CtrlReg;
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
         | (DataMode << CCR_DMODE_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (CmdMode  << CCR_IMODE_BIT)
         ;
  CtrlReg  = OCTOSPI_CR;
  CtrlReg &= ~(CR_FMODE_MASK << CR_FMODE_BIT);
  CtrlReg |=   CR_FMODE_READ << CR_FMODE_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
  _ClearFlags();
#if FS_NOR_HW_USE_OS
  _StatusOSPI = 0;
#endif // FS_NOR_HW_USE_OS
  NumBytesToRead = NumBytesData - 1;        // We have to subtract 1 because 0 means "read 1 byte".
  if (NumBytesData) {
    OCTOSPI_DLR = NumBytesToRead;
  }
  OCTOSPI_CR  = CtrlReg;
  OCTOSPI_ABR = AltReg;
  OCTOSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    OCTOSPI_IR = Cmd;
    OCTOSPI_AR = AddrReg;
  } else {
    OCTOSPI_IR = Cmd;
  }
  //
  // Read data from NOR flash.
  //
  if (NumBytesData != 0u) {
    (void)_ReadDataViaCPU(pData, NumBytesData);
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
  U32 CtrlReg;
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
         | (DataMode << CCR_DMODE_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (CmdMode  << CCR_IMODE_BIT)
         ;
  CtrlReg  = OCTOSPI_CR;
  CtrlReg &= ~(CR_FMODE_MASK  << CR_FMODE_BIT);
  CtrlReg |=   CR_FMODE_WRITE << CR_FMODE_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
  }
  //
  // Execute the command.
  //
#if FS_NOR_HW_USE_OS
  _StatusOSPI = 0;
#endif // FS_NOR_HW_USE_OS
  _ClearFlags();
  if (NumBytesData != 0u) {
    OCTOSPI_DLR = NumBytesData - 1;       // 0 means "write 1 byte".
  }
  OCTOSPI_CR  = CtrlReg;
  OCTOSPI_ABR = AltReg;
  OCTOSPI_CCR = CfgReg;
  if (NumBytesAddr) {
    OCTOSPI_IR = Cmd;
    OCTOSPI_AR = AddrReg;
  } else {
    OCTOSPI_IR = Cmd;
  }
  //
  // Write data to NOR flash.
  //
  if (NumBytesData) {
    (void)_WriteDataViaCPU(pData, NumBytesData);
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
    U32 CtrlReg;
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
           | (DataMode << CCR_DMODE_BIT)
           | (CmdMode  << CCR_IMODE_BIT)
           ;
    CtrlReg  =   OCTOSPI_CR;
    CtrlReg &= ~(CR_FMODE_MASK << CR_FMODE_BIT);
    CtrlReg |=   CR_FMODE_POLL << CR_FMODE_BIT;
    //
    // Wait until the unit is ready for the new command.
    //
    for (;;) {
      if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
        break;
      }
    }
    //
    // Start the poll operation. The end of operation is signaled by interrupt.
    //
    _ClearFlags();
    _StatusOSPI = 0;
    if (NumBytes) {
      OCTOSPI_DLR = NumBytes - 1;             // 0 means "read 1 byte".
    }
    OCTOSPI_CR    = CtrlReg;
    OCTOSPI_PSMKR = DataMask;
    OCTOSPI_PSMAR = DataMatch;
    OCTOSPI_PIR   = Delay;
    OCTOSPI_FCR   = 1uL << SR_SMF_BIT;
    OCTOSPI_CCR   = CfgReg;
    OCTOSPI_IR    = Cmd;
    for (;;) {
      Status = _StatusOSPI;
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
*       Public data
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS

/*********************************************************************
*
*       OCTOSPI2_IRQHandler
*/
void OCTOSPI2_IRQHandler(void);
void OCTOSPI2_IRQHandler(void) {
  U32 Status;

  OS_EnterInterrupt();
  Status       = OCTOSPI_SR;
  _StatusOSPI |= Status;
  OCTOSPI_FCR  = Status;            // Prevent other interrupts from occurring.
  FS_X_OS_Signal();
  OS_LeaveInterrupt();
}

#endif // FS_NOR_HW_USE_OS

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32L4R9_ST_STM32L4R9I_DISCO = {
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
