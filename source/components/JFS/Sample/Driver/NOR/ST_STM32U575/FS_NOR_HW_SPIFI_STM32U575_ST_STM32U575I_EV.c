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

File    : FS_NOR_HW_SPIFI_STM32U575_ST_STM32U575I_EV.c
Purpose : Low-level flash driver for STM32U5 OCTO SPI interface.
Literature:
  [1] RM0456 Reference manual STM32U575/585 Arm-based 32-bit MCUs
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32U5\RM0456-STM32U575585.pdf)
  [2] Datasheet STM32U575xx
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32U5\stm32u575xx.pdf)
  [3] Errata sheet STM32U575xx STM32U585xx
    (\\fileserver\Techinfo_rw\Company\ST\MCU\STM32\STM32U5\ES0499_Rev_0.3_Pub.pdf)
  [4] UM2854 User manual STM32U575I-EV
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32U5\EvalBoard\STM32U575I-EV\um2854-evaluation-board-with-stm32u575ai-mcu-stmicroelectronics.pdf)
  [5] Datasheet MX25LM51245G
    ("\\FILESERVER\Techinfo\Company\Macronix\SPI_NOR_Flash\MX25LM51245G, 3V, 512Mb, v1.1.pdf")

Additional information:
  ST recommends removing the camera board when testing with the
  OCTOSPI NOR flash device because some I/Os are shared.
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
#ifndef   FS_NOR_HW_OCTOSPI_CLK_HZ
  #define FS_NOR_HW_OCTOSPI_CLK_HZ      160000000uL     // Frequency of the clock supplied to OCTOSPI peripheral
#endif

#ifndef   FS_NOR_HW_NOR_CLK_HZ
  #define FS_NOR_HW_NOR_CLK_HZ          40000000uL      // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS              0               // Enables/disables the interrupt-driven operation.
                                                        // Permitted values:
                                                        //   0 - polling via CPU
                                                        //   1 - event-driven using embOS
                                                        //   2 - event-driven using other RTOS
#endif

#ifndef   FS_NOR_HW_USE_MEM_MODE
  #define FS_NOR_HW_USE_MEM_MODE        1               // Enables or disables the reading of NOR flash data via system memory.
                                                        // Has to be set to 0 if the file system is configured with FS_NOR_LINE_SIZE
                                                        // smaller than or equal to 2 because of the errata 2.6.7 in ES0499 Rev. 3.
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA             0               // Enables/disables the DMA support.
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
  #include "stm32u5xx.h"
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
*       Octo-SPI registers
*/
#define OCTOSPI_BASE_ADDR       0x420D2400uL
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
#define OCTOSPI_DR_HWORD        (*(volatile U16*)(OCTOSPI_BASE_ADDR + 0x050))
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
*       General-purpose DMA
*/
#define GPDMA_BASE_ADDR         0x40020000uL
#define GPDMA_CHANNEL_INDEX     0
#define GPDMA_SECCFGR           (*(volatile U32*)(GPDMA_BASE_ADDR + 0x000))
#define GPDMA_PRIVCFGR          (*(volatile U32*)(GPDMA_BASE_ADDR + 0x004))
#define GPDMA_RCFGLOCKR         (*(volatile U32*)(GPDMA_BASE_ADDR + 0x008))
#define GPDMA_MISR              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x00C))
#define GPDMA_SMISR             (*(volatile U32*)(GPDMA_BASE_ADDR + 0x010))
#define GPDMA_CLBAR             (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x50))
#define GPDMA_CFCR              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x5C))
#define GPDMA_CSR               (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x60))
#define GPDMA_CCR               (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x64))
#define GPDMA_CTR1              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x90))
#define GPDMA_CTR2              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x94))
#define GPDMA_CBR1              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x98))
#define GPDMA_CSAR              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0x9C))
#define GPDMA_CDAR              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0xA0))
#define GPDMA_CTR3              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0xA4))
#define GPDMA_CBR2              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0xA8))
#define GPDMA_CLLR              (*(volatile U32*)(GPDMA_BASE_ADDR + 0x80 * GPDMA_CHANNEL_INDEX + 0xCC))

/*********************************************************************
*
*       Port F registers
*/
#define GPIOF_BASE_ADDR         0x42021400uL
#define GPIOF_MODER             (*(volatile U32*)(GPIOF_BASE_ADDR + 0x00))
#define GPIOF_OSPEEDR           (*(volatile U32*)(GPIOF_BASE_ADDR + 0x08))
#define GPIOF_PUPDR             (*(volatile U32*)(GPIOF_BASE_ADDR + 0x0C))
#define GPIOF_ODR               (*(volatile U32*)(GPIOF_BASE_ADDR + 0x14))
#define GPIOF_AFRH              (*(volatile U32*)(GPIOF_BASE_ADDR + 0x24))
#define GPIOF_AFRL              (*(volatile U32*)(GPIOF_BASE_ADDR + 0x20))

/*********************************************************************
*
*       Port H registers
*/
#define GPIOH_BASE_ADDR         0x42021C00uL
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
#define GPIOI_BASE_ADDR         0x42022000uL
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
#define RCC_BASE_ADDR           0x46020C00uL
#define RCC_AHB2RSTR2           (*(volatile U32*)(RCC_BASE_ADDR + 0x68))
#define RCC_AHB1ENR             (*(volatile U32*)(RCC_BASE_ADDR + 0x88))
#define RCC_AHB2ENR1            (*(volatile U32*)(RCC_BASE_ADDR + 0x8C))
#define RCC_AHB2ENR2            (*(volatile U32*)(RCC_BASE_ADDR + 0x90))
#define RCC_CCIPR2              (*(volatile U32*)(RCC_BASE_ADDR + 0xE4))

/*********************************************************************
*
*       Masks for the peripheral enable bits
*/
#define AHB1ENR_GPDMA1EN        0
#define AHB1ENR_DCACHE1EN       30
#define AHB2ENR1_GPIOFEN        5
#define AHB2ENR1_GPIOHEN        7
#define AHB2ENR1_GPIOIEN        8
#define AHB2ENR1_OCTOSPIMEN     21
#define AHB2ENR2_OCTOSPI2EN     8
#define CCIPR2_OCTOSPISEL_BIT   20
#define CCIPR2_OCTOSPISEL_MASK  0x3uL

/*********************************************************************
*
*       GPIO bit positions of SPI signals
*/
#define NOR_CS_BIT              5     // Port I
#define NOR_DQS_BIT             4     // Port H
#define NOR_CLK_BIT             6     // Port H
#define NOR_D0_BIT              3     // Port I
#define NOR_D1_BIT              2     // Port I
#define NOR_D2_BIT              1     // Port I
#define NOR_D3_BIT              8     // Port H
#define NOR_D4_BIT              9     // Port H
#define NOR_D5_BIT              10    // Port H
#define NOR_D6_BIT              11    // Port H
#define NOR_D7_BIT              12    // Port H

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
#define DCR1_MTYP_BIT           24
#define DCR1_MTYP_MICRON        0uL
#define DCR1_MTYP_MACRONIX      1uL
#define DCR1_MTYP_STANDARD      2uL
#define DCR1_MTYP_MASK          0x7uL

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
#define CCR_IDTR_BIT            3
#define CCR_ISIZE_BIT           4
#define CCR_ISIZE_MASK          0x03uL
#define CCR_ADMODE_BIT          8
#define CCR_ADDTR_BIT           11
#define CCR_ADSIZE_BIT          12
#define CCR_ADSIZE_MASK         0x03uL
#define CCR_ABMODE_BIT          16
#define CCR_ABDTR_BIT           19
#define CCR_ABSIZE_BIT          20
#define CCR_ABSIZE_MASK         0x03uL
#define CCR_DMODE_BIT           24
#define CCR_DMODE_NONE          0x00uL
#define CCR_DMODE_SINGLE        0x01uL
#define CCR_DMODE_DUAL          0x02uL
#define CCR_DMODE_QUAD          0x03uL
#define CCR_DMODE_OCTO          0x04uL
#define CCR_DDTR_BIT            27
#define CCR_DQSE_BIT            29

/*********************************************************************
*
*       DMA channel control
*/
#define CCR_EN_BIT              0
#define CCR_RESET_BIT           1
#define CCR_SUSP_BIT            2
#define CCR_TCIE_BIT            8
#define CCR_DTEIE_BIT           10
#define CCR_USEIE_BIT           12
#define CCR_TOIE_BIT            14
#define CCR_PRIO_BIT            22
#define CCR_PRIO_LOW            0uL

/*********************************************************************
*
*       DMA channel status
*/
#define CSR_IDLEF_BIT           0
#define CSR_TCF_BIT             8
#define CSR_DTEF_BIT            10
#define CSR_USEF_BIT            12
#define CSR_SUSPF_BIT           13
#define CSR_TOF_BIT             14

/*********************************************************************
*
*       DMA channel flag clear
*/
#define CFCR_TCF_BIT            8
#define CFCR_HTF_BIT            9
#define CFCR_DTEF_BIT           10
#define CFCR_ULEF_BIT           11
#define CFCR_USEF_BIT           12
#define CFCR_SUSPF_BIT          13
#define CFCR_TOF_BIT            14

/*********************************************************************
*
*       DMA channel transfer settings
*/
#define CTR1_SDW_LOG2_BIT       0
#define CTR1_SDW_LOG2_WORD      2uL
#define CTR1_SINC_BIT           3
#define CTR1_DDW_LOG2_WORD      2uL
#define CTR1_DDW_LOG2_BIT       16
#define CTR1_DINC_BIT           19
#define CTR1_DBL_1_BIT          20
#define CTR2_DREQ_BIT           9
#define CTR2_REQSEL_BIT         0
#define CTR2_REQSEL_OCTOSPI2    41

/*********************************************************************
*
*       Octo-SPI timing configuration register
*/
#define TCR_DCYC_BIT            0
#define TCR_DCYC_MASK           0x1FuL
#define TCR_DHQC_BIT            28

/*********************************************************************
*
*       Misc. defines
*/
#define NUM_BYTES_FIFO          16
#define OCTOSPI_PRIO            15
#define GPDMA1_PRIO             15
#define WAIT_TIMEOUT_MS         1000
#define CYCLES_PER_MS           150           // Blocks the execution for about 1ms on a CPU running at 78 Mhz.
#define WAIT_TIMEOUT_CYCLES     (WAIT_TIMEOUT_MS * CYCLES_PER_MS)
#define CR2_IOSV_BIT            9
#define LOW_POWER_TIMEOUT       32

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static            U32 _ClkDiv;
#if FS_NOR_HW_USE_OS
  static volatile U32 _StatusOSPI;
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
*
*  Function description
*    Calculates the clock divider of Octo-SPI controller.
*
*  Parameters
*    pFreq_Hz     [OUT] Clock frequency.
*
*  Return value
*    Value of clock divisor.
*/
static U32 _CalcClockDivider(U32 * pFreq_Hz) {
  U32 Div;
  U32 Freq_Hz;
  U32 MaxFreq_Hz;

  //
  // Calculate the divider for the specified QSPI frequency.
  //
  Div        = 0;
  MaxFreq_Hz = *pFreq_Hz;
  for (;;) {
    Freq_Hz = FS_NOR_HW_OCTOSPI_CLK_HZ / (Div + 1u);
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
*       _CalcTransferMode
*
*  Function description
*    Calculates the transfer mode of Octo-SPI controller.
*
*  Parameters
*    BusWidth       Number of data lines to be used for the data transfer.
*    NumBytes       Number of bytes to be transferred. Can be 0.
*
*  Return value
*    Value of transfer mode.
*/
static U32 _CalcTransferMode(unsigned BusWidth, U32 NumBytes) {
  U32 Mode;

  Mode = CCR_DMODE_NONE;
  if (NumBytes != 0) {
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
*       _CalcNumCycles
*
*  Function description
*    Calculates the number of clock cycles required
*    to transfer a specified number of bytes.
*
*  Parameters
*    BusWidth       Number of data lines to be used for the data transfer.
*    NumBytes       Number of bytes to be transferred. Can be 0.
*    IsDTRMode      Indicates if the data is transferred on both clock cycles.
*
*  Return value
*    Number of clock cycles.
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
    case 8:
      NumCycles >>= 3;
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
*       _CalcMemType
*
*  Function description
*    Calculates the memory type of the NOR flash device.
*
*  Parameters
*    Flags      Operation flags.
*
*  Return value
*    Memory type.
*
*  Additional information
*    MTYPE = 0 (Micron)    FS_NOR_HW_FLAG_DQS_INVERTED = 1, FS_NOR_SPIFI_FLAG_DTR_D1_D0 = 0
*    MTYPE = 1 (Macronix)  FS_NOR_HW_FLAG_DQS_INVERTED = 0, FS_NOR_SPIFI_FLAG_DTR_D1_D0 = 1
*    MTYPE = 2 (standard)  FS_NOR_HW_FLAG_DQS_INVERTED = 0, FS_NOR_SPIFI_FLAG_DTR_D1_D0 = 0
*/
static U32 _CalcMemType(unsigned Flags) {
  U32 MemType;

  MemType = DCR1_MTYP_STANDARD;
  if ((Flags & FS_NOR_HW_FLAG_DQS_INVERTED) != 0) {
    if ((Flags & FS_NOR_HW_FLAG_DTR_D1_D0) == 0) {
      MemType = DCR1_MTYP_MICRON;
    }
  } else {
    if ((Flags & FS_NOR_HW_FLAG_DTR_D1_D0) != 0) {
      MemType = DCR1_MTYP_MACRONIX;
    }
  }
  return MemType;
}

/*********************************************************************
*
*       _WaitForTransferComplete
*
*  Function description
*    Waits for a data transfer to complete.
*
*  Return value
*    ==0      OK, transfer completed successfully.
*    !=0      An error occurred.
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
*
*  Function description
*    Waits for data to be received from NOR flash device.
*
*  Parameters
*    NumBytesExpected   Number of bytes expected to be received.
*
*  Return value
*    ==0      OK, specified number of bytes received.
*    !=0      An error occurred.
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
*
*  Function description
*    Waits for data to be sent to NOR flash device.
*
*  Parameters
*    NumBytesExpected   Number of bytes expected to be sent.
*
*  Return value
*    ==0      OK, specified number of bytes sent.
*    !=0      An error occurred.
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
*
*  Function description
*    Sets all the status flags to 0.
*/
static int _ClearFlags(void) {
  U32 TimeOut;
  int r;

  OCTOSPI_FCR = 0
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
    if ((OCTOSPI_SR & ((1uL << SR_TEF_BIT) |
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
*    Stops an ongoing data transfer.
*/
static int _Cancel(void) {
  U32 TimeOut;
  int r;

  r       = 0;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) != 0) {
    OCTOSPI_CR |= 1uL << CR_ABORT_BIT;
    _WaitForTransferComplete();
    OCTOSPI_FCR = 1uL << SR_TCF_BIT;
    for (;;) {
      if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
        break;
      }
      if (--TimeOut == 0u) {
        r = 1;                                // Error, could not cancel data transfer.
        break;
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _Reset
*
*  Function description
*    Resets the Octo-SPI controller
*/
static void _Reset(void) {
  RCC_AHB2RSTR2 |=  (1uL << AHB2ENR2_OCTOSPI2EN);
  RCC_AHB2RSTR2 &= ~(1uL << AHB2ENR2_OCTOSPI2EN);
  OCTOSPI_CR   = 0
               | (1uL << CR_EN_BIT)                           // Enable the Octo-SPI unit.
               | (1uL << CR_TCEN_BIT)                         // Enable the timeout for the memory mapped read operations.
               ;
  OCTOSPI_DCR1 = 0
               | (DCR1_DEVSIZE_MAX   << DCR1_DEVSIZE_BIT)     // We set the NOR flash size to maximum since this value is not known at this stage.
               | (2uL                << DCR1_CSHT_BIT)        // The CS signal stays 3 clock cycles high between commands. This setting was taken from an ST sample.
               | (DCR1_MTYP_STANDARD << DCR1_MTYP_BIT)
               ;
  OCTOSPI_DCR2 = 0
               | (_ClkDiv << DCR2_PRESCALER_BIT)              // Configure the clock frequency of the NOR flash.
               ;
  if (_ClkDiv != 0) {                                         // It is not permitted to set the DHQC bit if the prescaler is 0 (see page 944 in [1]).
    OCTOSPI_TCR  = 0
                 | (1uL << TCR_DHQC_BIT)                      // This setting was taken from an ST sample.
                 ;
  }
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
*
*  Function description
*    Transfers data from NOR flash device to CPU.
*
*  Parameters
*    pData      [OUT] Data transferred.
*    NumBytes   Number of bytes to be transferred.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
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
    if (NumBytes >= 4) {
      //
      // Read 4 bytes at a time.
      //
      DataReg = OCTOSPI_DR;
      *pData++ = (U8)DataReg;
      *pData++ = (U8)(DataReg >> 8);
      *pData++ = (U8)(DataReg >> 16);
      *pData++ = (U8)(DataReg >> 24);
      NumBytes -= 4u;
    } else {
      if (NumBytes >= 2) {
        //
        // Read 2 bytes at a time.
        //
        DataReg = OCTOSPI_DR_HWORD;
        *pData++ = (U8)DataReg;
        *pData++ = (U8)(DataReg >> 8);
        NumBytes -= 2u;
      } else {
        //
        // Read single bytes.
        //
        do {
          *pData++ = OCTOSPI_DR_BYTE;
        } while (--NumBytes);
      }
    }
  } while (NumBytes != 0u);
  return r;
}

/*********************************************************************
*
*       _WriteDataViaCPU
*
*  Function description
*    Transfers data from CPU to NOR flash device.
*
*  Parameters
*    pData      [IN] Data to be transferred.
*    NumBytes   Number of bytes to be transferred.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
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
    if (NumBytes >= 4u) {
      //
      // Write 4 bytes at a time.
      //
      DataReg  = (U32)*pData++;
      DataReg |= (U32)*pData++ << 8;
      DataReg |= (U32)*pData++ << 16;
      DataReg |= (U32)*pData++ << 24;
      NumBytes -= 4u;
      OCTOSPI_DR = DataReg;
    } else {
      if (NumBytes >= 2) {
        //
        // Write 2 bytes at a time.
        //
        DataReg  = (U32)*pData++;
        DataReg |= (U32)*pData++ << 8;
        NumBytes -= 2u;
        OCTOSPI_DR_HWORD = (U16)DataReg;
      } else {
        //
        // Write single bytes.
        //
        do {
          OCTOSPI_DR_BYTE = *pData++;
        } while (--NumBytes);
      }
    }
  } while (NumBytes != 0u);
  return r;
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       _StartDMATransfer
*
*  Function description
*    Initiates a data transfer via DMA.
*
*  Parameters
*    pData      [IN] Data to be sent to NOR flash device.
*               [OUT] Received data from the NOR flash device.
*    NumBytes   Number of bytes to be transfered.
*    IsWrite    Set to 1 if data has to be sent to NOR flash device.
*
*  Notes
*    (1) According to [1]:
*        "The DMA and OCTOSPI must be configured in a coherent manner
*        regarding data length: FTHRES value must reflect the DMA burst size. "
*/
static void _StartDMATransfer(U32 * pData, U32 NumBytes, int IsWrite) {
  U32 SrcAddr;
  U32 DestAddr;
  U32 TimeOut;

  //
  // Wait for the previous data transfer to finish.
  //
  if ((GPDMA_CSR & (1uL << CSR_IDLEF_BIT)) == 0) {
    GPDMA_CCR |= 1uL << CCR_SUSP_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((GPDMA_CSR & (1uL << CSR_SUSPF_BIT)) != 0) {
        break;
      }
      if ((GPDMA_CSR & (1uL << CSR_IDLEF_BIT)) != 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  GPDMA_CCR |= 1uL << CCR_RESET_BIT;
  //
  // Clear all the interrupt flags.
  //
  GPDMA_CFCR = 0
             | (1uL << CFCR_TCF_BIT)
             | (1uL << CFCR_HTF_BIT)
             | (1uL << CFCR_DTEF_BIT)
             | (1uL << CFCR_ULEF_BIT)
             | (1uL << CFCR_USEF_BIT)
             | (1uL << CFCR_SUSPF_BIT)
             | (1uL << CFCR_TOF_BIT)
             ;
  //
  // Configure the DMA data transfer.
  //
  if (IsWrite != 0) {
    SrcAddr  = SEGGER_PTR2ADDR(pData);
    DestAddr = SEGGER_PTR2ADDR(&OCTOSPI_DR);
  } else {
    SrcAddr  = SEGGER_PTR2ADDR(&OCTOSPI_DR);
    DestAddr = SEGGER_PTR2ADDR(pData);
  }
  GPDMA_CSAR = SrcAddr;
  GPDMA_CDAR = DestAddr;
  GPDMA_CBR1 = NumBytes;
  GPDMA_CCR  = 0
             | (CCR_PRIO_LOW << CCR_PRIO_BIT)
             ;
  GPDMA_CTR1 = 0
             | (CTR1_DDW_LOG2_WORD << CTR1_DDW_LOG2_BIT)
             | (CTR1_SDW_LOG2_WORD << CTR1_SDW_LOG2_BIT)
             ;
  GPDMA_CTR2 = 0
             | (CTR2_REQSEL_OCTOSPI2 << CTR2_REQSEL_BIT)
             ;
  if (IsWrite == 0) {
    GPDMA_CTR1 |= 1uL << CTR1_DINC_BIT;
  } else {
    GPDMA_CTR1 |= 1uL << CTR1_SINC_BIT;
    GPDMA_CTR2 |= 1uL << CTR2_DREQ_BIT;
  }
#if FS_NOR_HW_USE_OS
  //
  // Enable the DMA interrupts.
  //
  GPDMA_CCR   |= 0
              | (1uL << CCR_TCIE_BIT)
              | (1uL << CCR_DTEIE_BIT)
              | (1uL << CCR_USEIE_BIT)
              | (1uL << CCR_TOIE_BIT)
              ;
  _StatusDMA  = 0;
#endif // FS_NOR_HW_USE_OS
  //
  // Configure OCTOSPI for DMA operation.
  //
  OCTOSPI_CR  |= 0u
              | ((4u - 1u) << CR_FTHRES_BIT)              // Signal the DMA when 4 bytes are available or free. Note 1.
              | (1uL       << CR_DMAEN_BIT)               // Enable the data transfer via DMA in QSPI controller.
              ;
  //
  // Start the data transfer via DMA.
  //
  GPDMA_CCR   |= 1uL << CCR_EN_BIT;
}

/*********************************************************************
*
*       _WaitForEndOfDMATransfer
*
*  Function description
*    Waits for a DMA transfer to finish.
*
*  Return value
*    ==0    OK, data transferred successfully.
*    !=0    An error occurred.
*/
static int _WaitForEndOfDMATransfer(void) {
  U32 StatusOSPI;
  U32 StatusDMA;
  int r;
  U32 TimeOut;

  r = 1;                    // Set to indicate an error.
#if FS_NOR_HW_USE_OS
  for (;;) {
    StatusOSPI = _StatusOSPI;
    StatusDMA  = _StatusDMA;
    if ((StatusDMA & (1uL << CSR_TOF_BIT)) != 0u) {
      break;                                              // Error, transfer overflow.
    }
    if ((StatusDMA & (1uL << CSR_USEF_BIT)) != 0) {
      break;                                              // Error, invalid configuration.
    }
    if ((StatusDMA & (1uL << CSR_DTEF_BIT)) != 0) {
      break;                                              // Error, could not transfer data.
    }
    if ((StatusOSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusOSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << CSR_TCF_BIT)) != 0) {
      r = 0;                                              // OK, data transferred.
      break;
    }
    r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
    if (r != 0) {
      break;
    }
  }
#else
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    StatusOSPI = OCTOSPI_SR;
    StatusDMA  = GPDMA_CSR;
    if ((StatusDMA & (1uL << CSR_TOF_BIT)) != 0u) {
      break;                                              // Error, transfer overflow.
    }
    if ((StatusDMA & (1uL << CSR_USEF_BIT)) != 0) {
      break;                                              // Error, invalid configuration.
    }
    if ((StatusDMA & (1uL << CSR_DTEF_BIT)) != 0) {
      break;                                              // Error, could not transfer data.
    }
    if ((StatusOSPI & (1uL << SR_TEF_BIT)) != 0) {
      break;                                              // Error, invalid address.
    }
    if ((StatusOSPI & (1uL << SR_TOF_BIT)) != 0) {
      break;                                              // Error, a timeout occurred.
    }
    if ((StatusDMA & (1uL << CSR_TCF_BIT)) != 0) {
      r = 0;                                              // OK, data transferred.
      break;
    }
    if (--TimeOut == 0) {
      break;                                              // Error, a timeout occurred.
    }
  }
#endif // FS_NOR_HW_USE_OS
  if (r != 0) {
    //
    // Stop the DMA transfer in case of an error.
    //
    if ((GPDMA_CSR & (1uL << CSR_IDLEF_BIT)) == 0) {
      GPDMA_CCR |= 1uL << CCR_SUSP_BIT;
      TimeOut = WAIT_TIMEOUT_CYCLES;
      for (;;) {
        if ((GPDMA_CSR & (1uL << CSR_SUSPF_BIT)) != 0) {
          break;
        }
        if ((GPDMA_CSR & (1uL << CSR_IDLEF_BIT)) != 0) {
          break;
        }
        if (--TimeOut == 0) {
          break;
        }
      }
    }
    GPDMA_CCR |= 1uL << CCR_RESET_BIT;
  }
  OCTOSPI_CR &= ~((1uL            << CR_DMAEN_BIT) |      // Disable the data transfer via DMA in QSPI controller and reset the FIFO threshold.
                  (CR_FTHRES_MASK << CR_FTHRES_BIT));
  return r;
}

/*********************************************************************
*
*      _IsDataAligned
*
*  Function description
*    Checks if the memory block is word aligned.
*
*  Parameters
*    pData      Address of the memory block.
*    NumBytes   Size of the memory block in bytes.
*
*  Return value
*    !=0    The memory block is word aligned.
*    ==0    The memory block is not word aligned.
*/
static int _IsDataAligned(const void * pData, U32 NumBytes) {
  if ((((U32)pData & 3u) == 0u) && ((NumBytes & 3u) == 0u)) {
    return 1;
  }
  return 0;
}

#endif // FS_NOR_HW_USE_DMA

#if FS_NOR_HW_USE_MEM_MODE

/*********************************************************************
*
*       _Map
*
*  Function description
*    Configures the hardware for access to serial NOR flash via system memory.
*/
static int _Map(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U16 BusWidth, unsigned Flags) {
  U32 CfgReg;
  U32 CtrlReg;
  U32 InstMode;
  U32 AddrMode;
  U32 DataMode;
  U32 NumCyclesDummy;
  U32 AddrSize;
  U32 TCfgReg;
  U32 InstReg;
  U32 InstSize;
  U32 NumBytes;
  U32 InstDTR;
  U32 AddrDTR;
  U32 DataDTR;
  U32 MemType;
  U32 DevCfgReg;
  U32 TimeOut;
  int r;
  U32 AltReg;
  U32 NumBytesAlt;
  U32 AltMode;
  U32 AltSize;

  FS_USE_PARA(Unit);
  FS_USE_PARA(pPara);
  //
  // Fill local variables.
  //
  InstReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Calculate the transfer mode.
  //
  InstDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0u) {
    InstDTR = 1;
  }
  AddrDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u) {
    AddrDTR = 1;
  }
  DataDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u) {
    DataDTR = 1;
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
        || ((Flags & FS_NOR_HW_FLAG_MODE_4BIT) != 0u)) {                            // OCTOSPI is not able to send a number of mode bits that is not a multiple of eight.
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, AddrDTR);
    }
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  InstMode = _CalcTransferMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _CalcTransferMode(FS_BUSWIDTH_GET_DATA(BusWidth), 1);                  // We read at least one byte.
  //
  // Encode the instruction.
  //
  NumBytes = NumBytesCmd;
  do {
    InstReg <<= 8;
    InstReg  |= (U32)(*pCmd++);
  } while (--NumBytes);
  //
  // Calculate the number of bytes in each command phase.
  //
  InstSize = 0;
  if (NumBytesCmd) {
    InstSize = (NumBytesCmd - 1) & CCR_ISIZE_MASK;
  }
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
         | (DataMode << CCR_DMODE_BIT)
         | (DataDTR  << CCR_DDTR_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AddrDTR  << CCR_ABDTR_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrDTR  << CCR_ADDTR_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (InstMode << CCR_IMODE_BIT)
         | (InstDTR  << CCR_IDTR_BIT)
         | (InstSize << CCR_ISIZE_BIT)
         | (DataDTR  << CCR_DQSE_BIT)
         ;
  MemType    = _CalcMemType(Flags);
  DevCfgReg  =   OCTOSPI_DCR1;
  DevCfgReg &= ~(DCR1_MTYP_MASK << DCR1_MTYP_BIT);
  DevCfgReg |=   MemType        << DCR1_MTYP_BIT;
  CtrlReg    =   OCTOSPI_CR;
  CtrlReg   &= ~(CR_FMODE_MASK << CR_FMODE_BIT);
  CtrlReg   |=   CR_FMODE_MMAP << CR_FMODE_BIT;
  TCfgReg    =   OCTOSPI_TCR;
  TCfgReg   &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  TCfgReg   |=   NumCyclesDummy << TCR_DCYC_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                                        // Error, OCTOSPI is not ready.
      goto Done;
    }
  }
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                                                                      // Error, could not clear status flags.
  }
  //
  // Start the operation.
  //
  OCTOSPI_DCR1 = DevCfgReg;
  OCTOSPI_CR   = CtrlReg;
  OCTOSPI_ABR  = AltReg;
  OCTOSPI_TCR  = TCfgReg;
  OCTOSPI_AR   = 0;
  OCTOSPI_CCR  = CfgReg;
  OCTOSPI_IR   = InstReg;
Done:
  return r;
}

#endif // FS_NOR_HW_USE_MEM_MODE

/*********************************************************************
*
*       _Control
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*/
static int _Control(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, U8 BusWidth, unsigned Flags) {
  U32 CfgReg;
  U32 CtrlReg;
  U32 InstMode;
  U32 InstReg;
  U32 InstSize;
  U32 NumBytes;
  U32 InstDTR;
  U32 TCfgReg;
  U32 MemType;
  U32 DevCfgReg;
  int r;
  U32 TimeOut;

  FS_USE_PARA(Unit);
  //
  // Calculate the transfer mode.
  //
  InstDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0u) {
    InstDTR = 1;
  }
  //
  // Fill local variables.
  //
  InstReg  = 0;
  InstMode = _CalcTransferMode(BusWidth, NumBytesCmd);
  //
  // Encode the instruction.
  //
  NumBytes = NumBytesCmd;
  do {
    InstReg <<= 8;
    InstReg  |= (U32)(*pCmd++);
  } while (--NumBytes);
  //
  // Calculate the number of bytes in the instruction.
  //
  InstSize = 0;
  if (NumBytesCmd) {
    InstSize = (NumBytesCmd - 1) & CCR_ISIZE_MASK;
  }
  //
  // Configure the operation.
  //
  CfgReg  = 0
          | (InstMode << CCR_IMODE_BIT)
          | (InstDTR  << CCR_IDTR_BIT)
          | (InstSize << CCR_ISIZE_BIT)
          ;
  MemType    = _CalcMemType(Flags);
  DevCfgReg  =   OCTOSPI_DCR1;
  DevCfgReg &= ~(DCR1_MTYP_MASK << DCR1_MTYP_BIT);
  DevCfgReg |=   MemType        << DCR1_MTYP_BIT;
  CtrlReg    =   OCTOSPI_CR;
  CtrlReg   &= ~(CR_FMODE_MASK  << CR_FMODE_BIT);
  CtrlReg   |=   CR_FMODE_WRITE << CR_FMODE_BIT;
  TCfgReg    =   OCTOSPI_TCR;
  TCfgReg   &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                // Error, OCTOSPI is not ready.
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
  OCTOSPI_DCR1 = DevCfgReg;
  OCTOSPI_CR   = CtrlReg;
  OCTOSPI_DLR  = 0;
  OCTOSPI_ABR  = 0;
  OCTOSPI_CCR  = CfgReg;
  OCTOSPI_TCR  = TCfgReg;
  OCTOSPI_IR   = InstReg;
  //
  // Wait until the command has been completed.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                                                // Error, OCTOSPI is not ready.
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
  U32 CtrlReg;
  U32 DataMode;
  U32 AddrMode;
  U32 AltMode;
  U32 InstMode;
  U32 AltSize;
  U32 AddrSize;
  U32 NumBytes;
  U32 NumBytesAlt;
  U32 NumBytesToRead;
  U32 InstReg;
  U32 InstSize;
  U32 InstDTR;
  U32 AddrDTR;
  U32 DataDTR;
  U32 NumCyclesDummy;
  U32 TCfgReg;
  int r;
  int Result;
  U32 MemType;
  U32 DevCfgReg;
  U32 TimeOut;

  FS_USE_PARA(Unit);
  //
  // Calculate the transfer mode.
  //
  InstDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0u) {
    InstDTR = 1;
  }
  AddrDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u) {
    AddrDTR = 1;
  }
  DataDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u) {
    DataDTR = 1;
  }
  //
  // Fill local variables.
  //
  InstReg     = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Encode the instruction.
  //
  NumBytes = NumBytesCmd;
  do {
    InstReg <<= 8;
    InstReg  |= (U32)(*pCmd++);
  } while (--NumBytes);
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
  NumCyclesDummy = 0;
  if (NumBytesAlt != 0) {
    NumBytes = NumBytesAlt;
    //
    // Check if a mode byte has to be sent and if yes encode its value.
    //
    if (   ((Flags & FS_NOR_HW_FLAG_MODE_8BIT) != 0u)
        || ((Flags & FS_NOR_HW_FLAG_MODE_4BIT) != 0u)) {                            // OCTOSPI is not able to send a number of mode bits that is not a multiple of eight.
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, AddrDTR);
    }
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  InstMode = _CalcTransferMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _CalcTransferMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
  //
  // Calculate the number of bytes in each command phase.
  //
  InstSize = 0;
  if (NumBytesCmd) {
    InstSize = (NumBytesCmd - 1) & CCR_ISIZE_MASK;
  }
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
         | (DataMode << CCR_DMODE_BIT)
         | (DataDTR  << CCR_DDTR_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AddrDTR  << CCR_ABDTR_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrDTR  << CCR_ADDTR_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (InstMode << CCR_IMODE_BIT)
         | (InstDTR  << CCR_IDTR_BIT)
         | (InstSize << CCR_ISIZE_BIT)
         | (DataDTR  << CCR_DQSE_BIT)
         ;
  MemType    = _CalcMemType(Flags);
  DevCfgReg  =   OCTOSPI_DCR1;
  DevCfgReg &= ~(DCR1_MTYP_MASK << DCR1_MTYP_BIT);
  DevCfgReg |=   MemType        << DCR1_MTYP_BIT;
  CtrlReg    =   OCTOSPI_CR;
  CtrlReg   &= ~(CR_FMODE_MASK  << CR_FMODE_BIT);
  CtrlReg   |=   CR_FMODE_READ  << CR_FMODE_BIT;
  TCfgReg    =   OCTOSPI_TCR;
  TCfgReg   &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  TCfgReg   |=   NumCyclesDummy << TCR_DCYC_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, OCTOSPI is not ready.
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
#if FS_NOR_HW_USE_OS
  _StatusOSPI = 0;
#endif // FS_NOR_HW_USE_OS
  NumBytesToRead = NumBytesData - 1;        // We have to subtract 1 because 0 means "read 1 byte".
  if (NumBytesData != 0u) {
    OCTOSPI_DLR = NumBytesToRead;
  }
  OCTOSPI_DCR1 = DevCfgReg;
  OCTOSPI_CR   = CtrlReg;
  OCTOSPI_ABR  = AltReg;
  OCTOSPI_CCR  = CfgReg;
  OCTOSPI_TCR  = TCfgReg;
  OCTOSPI_IR   = InstReg;
  if (NumBytesAddr != 0u) {
    OCTOSPI_AR = AddrReg;
  }
  //
  // Receive the data from NOR flash device.
  //
  r = 0;
  if (NumBytesData != 0u) {
#if FS_NOR_HW_USE_DMA
    if (_IsDataAligned(pData, NumBytesData) != 0) {
      _StartDMATransfer((U32 *)pData, NumBytesData, 0);
      Result = _WaitForEndOfDMATransfer();
    } else
#endif // FS_NOR_HW_USE_DMA
    {
      Result = _ReadDataViaCPU(pData, NumBytesData);
    }
    if (Result != 0) {
      r = 1;
    }
  }
  //
  // Wait until the data transfer has been completed.
  //
  Result = _WaitForTransferComplete();
  if (Result != 0) {
    r = 1;
  }
Done:
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
  U32 CtrlReg;
  U32 DataMode;
  U32 AddrMode;
  U32 AltMode;
  U32 InstMode;
  U32 AddrSize;
  U32 AltSize;
  U32 NumBytes;
  U32 NumBytesAlt;
  U32 InstReg;
  U32 InstDTR;
  U32 AddrDTR;
  U32 DataDTR;
  int r;
  int Result;
  U32 InstSize;
  U32 TCfgReg;
  U32 MemType;
  U32 DevCfgReg;
  U32 TimeOut;

  FS_USE_PARA(Unit);
  //
  // Calculate the transfer mode.
  //
  InstDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0u) {
    InstDTR = 1;
  }
  AddrDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u) {
    AddrDTR = 1;
  }
  DataDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u) {
    DataDTR = 1;
  }
  //
  // Fill local variables.
  //
  InstReg     = 0;
  AddrReg     = 0;
  AltReg      = 0;
  NumBytesAlt = NumBytesPara - NumBytesAddr;
  //
  // Encode the instruction.
  //
  NumBytes = NumBytesCmd;
  do {
    InstReg <<= 8;
    InstReg  |= (U32)(*pCmd++);
  } while (--NumBytes);
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
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  InstMode = _CalcTransferMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _CalcTransferMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
  //
  // Calculate the number of bytes in each command phase.
  //
  InstSize = 0;
  if (NumBytesCmd) {
    InstSize = (NumBytesCmd - 1) & CCR_ISIZE_MASK;
  }
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
         | (DataMode << CCR_DMODE_BIT)
         | (DataDTR  << CCR_DDTR_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AddrDTR  << CCR_ABDTR_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrDTR  << CCR_ADDTR_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (InstMode << CCR_IMODE_BIT)
         | (InstDTR  << CCR_IDTR_BIT)
         | (InstSize << CCR_ISIZE_BIT)
         ;
  MemType    = _CalcMemType(Flags);
  DevCfgReg  =   OCTOSPI_DCR1;
  DevCfgReg &= ~(DCR1_MTYP_MASK << DCR1_MTYP_BIT);
  DevCfgReg |=   MemType        << DCR1_MTYP_BIT;
  CtrlReg    =   OCTOSPI_CR;
  CtrlReg   &= ~(CR_FMODE_MASK  << CR_FMODE_BIT);
  CtrlReg   |=   CR_FMODE_WRITE << CR_FMODE_BIT;
  TCfgReg    =   OCTOSPI_TCR;
  TCfgReg   &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, OCTOSPI is not ready.
      goto Done;
    }
  }
  //
  // Execute the command.
  //
#if FS_NOR_HW_USE_OS
  _StatusOSPI = 0;
#endif // FS_NOR_HW_USE_OS
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                            // Error, could not clear status flags.
  }
  if (NumBytesData != 0u) {
    OCTOSPI_DLR = NumBytesData - 1;       // 0 means "write 1 byte".
  }
  OCTOSPI_DCR1 = DevCfgReg;
  OCTOSPI_CR   = CtrlReg;
  OCTOSPI_ABR  = AltReg;
  OCTOSPI_CCR  = CfgReg;
  OCTOSPI_TCR  = TCfgReg;
  OCTOSPI_IR   = InstReg;
  if (NumBytesAddr != 0u) {
    OCTOSPI_AR = AddrReg;
  }
  //
  // Send data to the NOR flash device.
  //
  r = 0;
  if (NumBytesData != 0u) {
#if FS_NOR_HW_USE_DMA
    if (_IsDataAligned(pData, NumBytesData) != 0) {
      _StartDMATransfer((U32 *)pData, NumBytesData, 1);
      Result = _WaitForEndOfDMATransfer();
    } else
#endif // FS_NOR_HW_USE_DMA
    {
      Result = _WriteDataViaCPU(pData, NumBytesData);
    }
    if (Result != 0) {
      r = 1;
    }
  }
  //
  // Wait until the data transfer has been completed.
  //
  Result = _WaitForTransferComplete();
  if (Result != 0) {
    r = 1;
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
  int r;
  U32 CfgReg;
  U32 CtrlReg;
  U32 DataMode;
  U32 InstMode;
  U32 NumBytes;
  U32 NumBytesData;
  U32 DataMask;
  U32 DataMatch;
  U32 Status;
  U32 InstReg;
  U32 AddrReg;
  U32 AltReg;
  U32 NumBytesAlt;
  U32 AddrMode;
  U32 AltMode;
  U32 AddrSize;
  U32 AltSize;
  U32 InstSize;
  U32 InstDTR;
  U32 AddrDTR;
  U32 DataDTR;
  U32 TCfgReg;
  U32 NumCyclesDummy;
  U32 MemType;
  U32 DevCfgReg;
  U32 TimeOut;

  FS_USE_PARA(Unit);
  //
  // Calculate the transfer mode.
  //
  InstDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0u) {
    InstDTR = 1;
  }
  AddrDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0u) {
    AddrDTR = 1;
  }
  DataDTR = 0;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0u) {
    DataDTR = 1;
  }
  //
  // Fill local variables.
  //
  InstReg        = 0;
  AddrReg        = 0;
  AltReg         = 0;
  DataMask       = 1uL           << BitPos;
  DataMatch      = (U32)BitValue << BitPos;
  Delay        <<= 4;                                                               // 16 clock cycles are required to transfer 2 bytes.
  NumBytesData   = 1;                                                               // The response consists of only one byte.
  NumBytesAlt    = NumBytesPara - NumBytesAddr;
  //
  // Encode the instruction.
  //
  NumBytes = NumBytesCmd;
  do {
    InstReg <<= 8;
    InstReg  |= (U32)(*pCmd++);
  } while (--NumBytes);
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
        || ((Flags & FS_NOR_HW_FLAG_MODE_4BIT) != 0u)) {                            // OCTOSPI is not able to send a number of mode bits that is not a multiple of eight.
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
      NumCyclesDummy = _CalcNumCycles(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytes, AddrDTR);
    }
  }
  //
  // Calculate the number of data lines to be used for the transfer.
  //
  InstMode = _CalcTransferMode(FS_BUSWIDTH_GET_CMD(BusWidth), NumBytesCmd);
  AddrMode = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAddr);
  AltMode  = _CalcTransferMode(FS_BUSWIDTH_GET_ADDR(BusWidth), NumBytesAlt);
  DataMode = _CalcTransferMode(FS_BUSWIDTH_GET_DATA(BusWidth), NumBytesData);
  //
  // Calculate the number of bytes in each command phase.
  //
  InstSize = 0;
  if (NumBytesCmd) {
    InstSize = (NumBytesCmd - 1) & CCR_ISIZE_MASK;
  }
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
         | (DataMode << CCR_DMODE_BIT)
         | (DataDTR  << CCR_DDTR_BIT)
         | (AltSize  << CCR_ABSIZE_BIT)
         | (AddrDTR  << CCR_ABDTR_BIT)
         | (AltMode  << CCR_ABMODE_BIT)
         | (AddrSize << CCR_ADSIZE_BIT)
         | (AddrDTR  << CCR_ADDTR_BIT)
         | (AddrMode << CCR_ADMODE_BIT)
         | (InstMode << CCR_IMODE_BIT)
         | (InstDTR  << CCR_IDTR_BIT)
         | (InstSize << CCR_ISIZE_BIT)
         ;
  MemType    = _CalcMemType(Flags);
  DevCfgReg  =   OCTOSPI_DCR1;
  DevCfgReg &= ~(DCR1_MTYP_MASK << DCR1_MTYP_BIT);
  DevCfgReg |=   MemType        << DCR1_MTYP_BIT;
  CtrlReg    =   OCTOSPI_CR;
  CtrlReg   &= ~(CR_FMODE_MASK << CR_FMODE_BIT);
  CtrlReg   |=   CR_FMODE_POLL << CR_FMODE_BIT;
  TCfgReg    = OCTOSPI_TCR;
  TCfgReg   &= ~(TCR_DCYC_MASK  << TCR_DCYC_BIT);
  TCfgReg   |=   NumCyclesDummy << TCR_DCYC_BIT;
  //
  // Wait until the unit is ready for the new command.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((OCTOSPI_SR & (1uL << SR_BUSY_BIT)) == 0) {
      break;
    }
    if (--TimeOut == 0u) {
      r = 1;                              // Error, OCTOSPI is not ready.
      goto Done;
    }
  }
  //
  // Start the poll operation. The end of operation is signaled via interrupt.
  //
  r = _ClearFlags();
  if (r != 0) {
    goto Done;                            // Error, could not clear status flags.
  }
  _StatusOSPI = 0;
  if (NumBytesData) {
    OCTOSPI_DLR = NumBytesData - 1;       // 0 means "read 1 byte".
  }
  OCTOSPI_DCR1  = DevCfgReg;
  OCTOSPI_ABR   = AltReg;
  OCTOSPI_CR    = CtrlReg;
  OCTOSPI_PSMKR = DataMask;
  OCTOSPI_PSMAR = DataMatch;
  OCTOSPI_PIR   = Delay;
  OCTOSPI_FCR   = 1uL << SR_SMF_BIT;
  OCTOSPI_CCR   = CfgReg;
  OCTOSPI_TCR   = TCfgReg;
  OCTOSPI_IR    = InstReg;
  if (NumBytesAddr != 0u) {
    OCTOSPI_AR = AddrReg;
  }
  for (;;) {
    Status = _StatusOSPI;
    if (Status & (1uL << SR_SMF_BIT)) {
      r = 0;                                // Bit set to the requested value.
      break;
    }
    r = FS_X_OS_Wait(TimeOut_ms);
    if (r != 0) {
      //
      // The timeout has expired. Cancel the polling operation.
      //
      (void)_Cancel();
      r = 1;                                // A timeout occurred.
      break;
    }
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
*    Initializes the SPI hardware.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*
*  Return value
*    > 0      OK, frequency of the SPI clock in Hz.
*    ==0      An error occurred.
*/
static int _HW_Init(U8 Unit) {
  U32 Freq_Hz;

  FS_USE_PARA(Unit);
  //
  // Select the clock source for OCTOSPI.
  //
  RCC_CCIPR2 &= ~(CCIPR2_OCTOSPISEL_MASK << CCIPR2_OCTOSPISEL_BIT);
  //
  // Enable the clocks of peripherals and reset them.
  //
  RCC_AHB2ENR1 |= 0
               | (1uL << AHB2ENR1_GPIOFEN)
               | (1uL << AHB2ENR1_GPIOHEN)
               | (1uL << AHB2ENR1_GPIOIEN)
               | (1uL << AHB2ENR1_OCTOSPIMEN)
               ;
  RCC_AHB1ENR  |= 0
               | (1uL << AHB1ENR_DCACHE1EN)         // Acc. to [1] it is necessary to enable this clock even when the data cache is not used.
#if FS_NOR_HW_USE_DMA
               | (1uL << AHB1ENR_GPDMA1EN)
#endif // FS_NOR_HW_USE_DMA
               ;
  RCC_AHB2ENR2 |= 1uL << AHB2ENR2_OCTOSPI2EN;
  //
  // Wait for the unit to exit reset.
  //
  for (;;) {
    if ((RCC_AHB2RSTR2 & (1uL << AHB2ENR2_OCTOSPI2EN)) == 0) {
      break;
    }
  }
  //
  // CS is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_CS_BIT       << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_CS_BIT       << 1));
  GPIOI_AFRL    &= ~(AFR_MASK     << ((NOR_CS_BIT - 0)  << 2));
  GPIOI_AFRL    |=  (0x5uL        << ((NOR_CS_BIT - 0)  << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_CS_BIT       << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_CS_BIT       << 1));
  //
  // DQS is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_DQS_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_DQS_BIT      << 1));
  GPIOH_AFRL    &= ~(AFR_MASK     << ((NOR_DQS_BIT - 0) << 2));
  GPIOH_AFRL    |=  (0x5uL        << ((NOR_DQS_BIT - 0) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_DQS_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_DQS_BIT      << 1));
  //
  // CLK is an output signal controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_CLK_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_CLK_BIT      << 1));
  GPIOH_AFRL    &= ~(AFR_MASK     << ((NOR_CLK_BIT - 0) << 2));
  GPIOH_AFRL    |=  (0x5uL        << ((NOR_CLK_BIT - 0) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_CLK_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_CLK_BIT      << 1));
  //
  // D0 is an input/output signal controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D0_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D0_BIT      << 1));
  GPIOI_AFRL    &= ~(AFR_MASK     << ((NOR_D0_BIT - 0) << 2));
  GPIOI_AFRL    |=  (0x6uL        << ((NOR_D0_BIT - 0) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D0_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D0_BIT      << 1));
  //
  // D1 is an input/output signal controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D1_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D1_BIT      << 1));
  GPIOI_AFRL    &= ~(AFR_MASK     << ((NOR_D1_BIT - 0) << 2));
  GPIOI_AFRL    |=  (0x6uL        << ((NOR_D1_BIT - 0) << 2));
  GPIOI_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D1_BIT      << 1));
  GPIOI_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D1_BIT      << 1));
  //
  // D2 is an input/output signal and is controlled by the OCTOSPI unit.
  //
  GPIOI_MODER   &= ~(MODER_MASK   << ( NOR_D2_BIT      << 1));
  GPIOI_MODER   |=  (MODER_ALT    << ( NOR_D2_BIT      << 1));
  GPIOI_AFRL    &= ~(AFR_MASK     << ((NOR_D2_BIT - 0) << 2));
  GPIOI_AFRL    |=  (0x6uL        << ((NOR_D2_BIT - 0) << 2));
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
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_D6_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_D6_BIT      << 1));
  GPIOH_AFRH    &= ~(AFR_MASK     << ((NOR_D6_BIT - 8) << 2));
  GPIOH_AFRH    |=  (0x5uL        << ((NOR_D6_BIT - 8) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D6_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D6_BIT      << 1));
  //
  // D7 is an output signal and is controlled by the OCTOSPI unit.
  //
  GPIOH_MODER   &= ~(MODER_MASK   << ( NOR_D7_BIT      << 1));
  GPIOH_MODER   |=  (MODER_ALT    << ( NOR_D7_BIT      << 1));
  GPIOH_AFRH    &= ~(AFR_MASK     << ((NOR_D7_BIT - 8) << 2));
  GPIOH_AFRH    |=  (0x5uL        << ((NOR_D7_BIT - 8) << 2));
  GPIOH_OSPEEDR &= ~(OSPEEDR_MASK << ( NOR_D7_BIT      << 1));
  GPIOH_OSPEEDR |=  (OSPEEDR_HIGH << ( NOR_D7_BIT      << 1));
#if FS_NOR_HW_USE_DMA
  //
  // Initialize DMA.
  //
  GPDMA_CLBAR = 0;
  GPDMA_CTR3  = 0;
  GPDMA_CBR2  = 0;
  GPDMA_CLLR  = 0;
#endif // FS_NOR_HW_USE_DMA
#if FS_NOR_HW_USE_OS
  //
  // Disable the interrupt and configure the priority.
  //
  NVIC_DisableIRQ(OCTOSPI2_IRQn);
  NVIC_SetPriority(OCTOSPI2_IRQn, OCTOSPI_PRIO);
#if FS_NOR_HW_USE_DMA
  NVIC_DisableIRQ(GPDMA1_Channel0_IRQn);
  NVIC_SetPriority(GPDMA1_Channel0_IRQn, GPDMA1_PRIO);
#endif // FS_NOR_HW_USE_DMA
#endif // FS_NOR_HW_USE_OS
  //
  // Initialize the Octo-SPI controller.
  //
  Freq_Hz = FS_NOR_HW_NOR_CLK_HZ;
  _ClkDiv = _CalcClockDivider(&Freq_Hz);
  _Reset();
#if FS_NOR_HW_USE_OS
  NVIC_EnableIRQ(OCTOSPI2_IRQn);
#if FS_NOR_HW_USE_DMA
  NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
#endif // FS_NOR_HW_USE_DMA
#endif // FS_NOR_HW_USE_OS
  return (int)Freq_Hz;
}

/*********************************************************************
*
*       _HW_Unmap
*
*  Function description
*    Configures the hardware for direct access to serial NOR flash.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_Unmap(U8 Unit) {
  FS_USE_PARA(Unit);

  //
  // Cancel the memory mode so that the BUSY bit goes to 0
  // and we can write to OCTOSPI_CCR register.
  //
  if ((OCTOSPI_CR & (CR_FMODE_MASK << CR_FMODE_BIT)) == (CR_FMODE_MMAP << CR_FMODE_BIT)) {
    _Cancel();
  }
  _Reset();
}

#if FS_NOR_HW_USE_MEM_MODE

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

#endif // FS_NOR_HW_USE_MEM_MODE

/*********************************************************************
*
*       _HW_Control
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    Cmd            Code of the command to be sent.
*    BusWidth       Number of data lines to be used for sending the command.
*/
static void _HW_Control(U8 Unit, U8 Cmd, U8 BusWidth) {
  (void)_Control(Unit, &Cmd, sizeof(Cmd), BusWidth, 0);
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Transfers data from serial NOR flash to MCU.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    Cmd            Code of the command to be sent.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    pData          [OUT] Data read from the serial NOR flash device. Can be NULL.
*    NumBytesData   Number of bytes to read from the serial NOR flash device. Can be 0.
*    BusWidth       Number of data lines to be used for the data transfer.
*/
static void _HW_Read(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  (void)_Read(Unit, &Cmd, sizeof(Cmd), pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth, 0);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Transfers data from MCU to serial NOR flash.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    Cmd            Code of the command to be sent.
*    pPara          [IN] Command parameters (address and dummy bytes). Can be NULL.
*    NumBytesPara   Total number of address and dummy bytes to be sent. Can be 0.
*    NumBytesAddr   Number of address bytes to be send. Can be 0.
*    pData          [IN] Data to be sent to the serial NOR flash device. Can be NULL.
*    NumBytesData   Number of bytes to be written the serial NOR flash device. Can be 0.
*    BusWidth       Number of data lines to be used for the data transfer.
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
*    Blocks the execution for the specified time.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    ms         Number of milliseconds to block the execution.
*
*  Return value
*    ==0    OK, delay executed.
*    < 0    Functionality not supported.
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

#if FS_NOR_HW_USE_MEM_MODE

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

#endif // FS_NOR_HW_USE_MEM_MODE

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

/*********************************************************************
*
*       Public code
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

#if (FS_NOR_HW_USE_OS == 1)
  OS_EnterNestableInterrupt();
#endif
  Status       = OCTOSPI_SR;
  _StatusOSPI |= Status;
  OCTOSPI_FCR  = Status;            // Prevent other interrupts from occurring.
  FS_X_OS_Signal();
#if (FS_NOR_HW_USE_OS == 1)
  OS_LeaveNestableInterrupt();
#endif
}

#if FS_NOR_HW_USE_DMA

/*********************************************************************
*
*       GPDMA1_Channel0_IRQHandler
*/
void GPDMA1_Channel0_IRQHandler(void);
void GPDMA1_Channel0_IRQHandler(void) {
  U32 Status;

#if (FS_NOR_HW_USE_OS == 1)
  OS_EnterNestableInterrupt();
#endif
  Status      = GPDMA_CSR;
  GPDMA_CFCR  = Status;         // Clear pending interrupt flags.
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
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_STM32U575_ST_STM32U575I_EV = {
  _HW_Init,
  _HW_Unmap,
#if FS_NOR_HW_USE_MEM_MODE
  _HW_Map,
#else
  NULL,
#endif
  _HW_Control,
  _HW_Read,
  _HW_Write,
  _HW_Poll,
  _HW_Delay,
  NULL,
  NULL
#if (FS_VERSION >= 51800u)
#if FS_NOR_HW_USE_MEM_MODE
  , _HW_MapEx
#else
  , NULL
#endif
  , _HW_ControlEx
  , _HW_ReadEx
  , _HW_WriteEx
  , _HW_PollEx
#endif
};

/*************************** End of file ****************************/
