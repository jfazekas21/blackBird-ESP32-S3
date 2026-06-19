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

File    : FS_NOR_HW_SPIFI_iMXRT1052_NXP_MIMXRT1050_EVK.c
Purpose : Low-level flash driver for NXP iMXRT1052 FlexSPI interface.
Literature:
  [1] i.MX RT1050 Processor Reference Manual
    (\\FILESERVER\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT10xx\iMXRT105x\IMXRT1050RM_Rev4.pdf)
  [2] i.MX RT1050 Applications Processors
    (\\FILESERVER\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT10xx\iMXRT105x\IMXRT1050XEC_Rev_B.pdf)
  [3] Schematics IMXRT1050-EVK
    (\\FILESERVER\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT10xx\iMXRT105x\Evalboard\MIMXRT1050-EVK\SPF-29538_A_170504.pdf)

Additional information:

  Signal name   GPIO            Alt. function
  -------------------------------------------
  FlexSPI_SS0   GPIO_SD_B1_06   ALT1
  FlexSPI_CLK   GPIO_SD_B1_07   ALT1
  FlexSPI_D0_A  GPIO_SD_B1_08   ALT1
  FlexSPI_D0_B  GPIO_SD_B1_09   ALT1
  FlexSPI_D0_C  GPIO_SD_B1_10   ALT1
  FlexSPI_D0_D  GPIO_SD_B1_11   ALT1
*/

/*********************************************************************
*
*      #include section
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
#ifndef   FS_NOR_HW_SPIFI_NOR_CLK_HZ
  #define FS_NOR_HW_SPIFI_NOR_CLK_HZ              (396000000 / 8)   // Frequency of the clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_SPIFI_FLEXSPI_MEM_ADDR
  #define FS_NOR_HW_SPIFI_FLEXSPI_MEM_ADDR        0x60000000        // This is the start address of the memory region used by the file system
                                                                    // to read the data from the serial NOR flash device. The hardware layer
                                                                    // uses this address to invalidate the data cache. It should be set to the value passed
                                                                    // as second parameter to FS_NOR_Configure()/FS_NOR_BM_Configure() in FS_X_AddDevices().
#endif

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       FlexSPI registers
*/
#define FLEXSPI_BASE_ADDR                     0x402A8000uL
#define FLEXSPI_MCR0                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x000))   // Module Control Register 0
#define FLEXSPI_MCR1                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x004))   // Module Control Register 1
#define FLEXSPI_MCR2                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x008))   // Module Control Register 2
#define FLEXSPI_AHBCR                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x00C))   // AHB Bus Control Register
#define FLEXSPI_INTEN                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x010))   // Interrupt Enable Register
#define FLEXSPI_INTR                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x014))   // Interrupt Register
#define FLEXSPI_LUTKEY                        (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x018))   // LUT Key Register
#define FLEXSPI_LUTCR                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x01C))   // LUT Control Register
#define FLEXSPI_AHBRXBUF0CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x020))   // AHB RX Buffer 0 Control Register 0
#define FLEXSPI_AHBRXBUF1CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x024))   // AHB RX Buffer 1 Control Register 0
#define FLEXSPI_AHBRXBUF2CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x028))   // AHB RX Buffer 2 Control Register 0
#define FLEXSPI_AHBRXBUF3CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x02C))   // AHB RX Buffer 3 Control Register 0
#define FLEXSPI_FLSHA1CR0                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x060))   // Flash A1 Control Register 0
#define FLEXSPI_FLSHA2CR0                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x064))   // Flash A2 Control Register 0
#define FLEXSPI_FLSHB1CR0                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x068))   // Flash B1 Control Register 0
#define FLEXSPI_FLSHB2CR0                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x06C))   // Flash B2 Control Register 0
#define FLEXSPI_FLSHA1CR1                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x070))   // Flash A1 Control Register 1
#define FLEXSPI_FLSHA2CR1                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x074))   // Flash A2 Control Register 1
#define FLEXSPI_FLSHB1CR1                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x078))   // Flash B1 Control Register 1
#define FLEXSPI_FLSHB2CR1                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x07C))   // Flash B2 Control Register 1
#define FLEXSPI_FLSHA1CR2                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x080))   // Flash A1 Control Register 2
#define FLEXSPI_FLSHA2CR2                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x084))   // Flash A2 Control Register 2
#define FLEXSPI_FLSHB1CR2                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x088))   // Flash B1 Control Register 2
#define FLEXSPI_FLSHB2CR2                     (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x08C))   // Flash B2 Control Register 2
#define FLEXSPI_FLSHCR4                       (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x094))   // Flash Control Register 4
#define FLEXSPI_IPCR0                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0A0))   // IP Control Register 0
#define FLEXSPI_IPCR1                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0A4))   // IP Control Register 1
#define FLEXSPI_IPCMD                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0B0))   // IP Command Register
#define FLEXSPI_IPRXFCR                       (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0B8))   // IP RX FIFO Control Register
#define FLEXSPI_IPTXFCR                       (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0BC))   // IP TX FIFO Control Register
#define FLEXSPI_DLLACR                        (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0C0))   // DLL Control Register 0
#define FLEXSPI_DLLBCR                        (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0C4))   // DLL Control Register 0
#define FLEXSPI_STS0                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0E0))   // Status Register 0
#define FLEXSPI_STS1                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0E4))   // Status Register 1
#define FLEXSPI_STS2                          (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0E8))   // Status Register 2
#define FLEXSPI_AHBSPNDSTS                    (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0EC))   // AHB Suspend Status Register
#define FLEXSPI_IPRXFSTS                      (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0F0))   // IP RX FIFO Status Register
#define FLEXSPI_IPTXFSTS                      (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x0F4))   // IP TX FIFO Status Register
#define FLEXSPI_RFDR0                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x100))   // IP RX FIFO Data Register
#define FLEXSPI_TFDR0                         (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x180))   // IP TX FIFO Data Register
#define FLEXSPI_LUT                           (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x200))   // LUT

/*********************************************************************
*
*       IOMUX registers
*/
#define IOMUXC_BASE_ADDR                      0x401F8000uL
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_06   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x1EC))
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_07   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x1F0))
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_08   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x1F4))
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_09   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x1F8))
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_10   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x1FC))
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_11   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x200))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_06   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3DC))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_07   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3E0))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_08   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3E4))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_09   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3E8))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_10   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3EC))
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_11   (*(volatile U32 *)(IOMUXC_BASE_ADDR + 0x3F0))

/*********************************************************************
*
*       Clock controller module registers
*/
#define CCM_BASE_ADDR                         0x400FC000uL
#define CCM_CSCMR1                            (*(volatile U32*)(CCM_BASE_ADDR + 0x1C))
#define CCM_CCGR4                             (*(volatile U32*)(CCM_BASE_ADDR + 0x78))
#define CCM_CCGR6                             (*(volatile U32*)(CCM_BASE_ADDR + 0x80))

/*********************************************************************
*
*       System Control Block
*/
#define SCB_BASE_ADDR                         0xE000E000uL
#define SCB_CCR                               (*(volatile U32 *)(SCB_BASE_ADDR + 0xD14))    // Configuration Control Register

/*********************************************************************
*
*       Memory protection unit
*/
#define MPU_BASE_ADDR                         0xE000ED90uL
#define MPU_TYPE                              (*(volatile U32 *)(MPU_BASE_ADDR + 0x00))
#define MPU_CTRL                              (*(volatile U32 *)(MPU_BASE_ADDR + 0x04))
#define MPU_RNR                               (*(volatile U32 *)(MPU_BASE_ADDR + 0x08))
#define MPU_RBAR                              (*(volatile U32 *)(MPU_BASE_ADDR + 0x0C))
#define MPU_RASR                              (*(volatile U32 *)(MPU_BASE_ADDR + 0x10))

/*********************************************************************
*
*       Module Control Register 0
*/
#define MCR0_MDIS_BIT                         1
#define MCR0_SWRESET_BIT                      0
#define MCR0_DOSEEN_BIT                       12

/*********************************************************************
*
*       IOMUX controller
*/
#define SW_MUX_CTL_MUX_MODE_MASK              0x7uL
#define SW_MUX_CTL_MUX_MODE_ALT1              0x1uL
#define SW_MUX_CTL_MUX_MODE_BIT               0
#define SW_PAD_CTL_SRE_BIT                    0
#define SW_PAD_CTL_DSE_BIT                    3
#define SW_PAD_CTL_SPEED_BIT                  6
#define SW_PAD_CTL_SPEED_MEDIUM               1uL
#define SW_PAD_CTL_SPEED_HIGH                 3uL
#define SW_PAD_CTL_PKE_BIT                    12

/*********************************************************************
*
*       MPU defines
*/
#define CTRL_ENABLE_BIT                       0
#define CTRL_PRIVDEFENA_BIT                   2
#define RNR_REGION_MASK                       0xFF
#define RASR_ENABLE_BIT                       0
#define RASR_SIZE_BIT                         1
#define RASR_TEX_BIT                          19
#define RASR_AP_BIT                           24
#define RASR_XN_BIT                           28
#define RASR_AP_FULL                          0x3uL
#define TYPE_DREGION_BIT                      8
#define TYPE_DREGION_MASK                     0xFFuL
#define CCR_DC_BIT                            16
#define RBAR_REGION_BIT                       0
#define RBAR_REGION_MASK                      0x1FuL
#define RBAR_VALID_BIT                        5

/*********************************************************************
*
*       Defines related to LUT
*/
#define LUT_OPCODE_BIT                        10
#define LUT_NUMPADS_BIT                       8
#define LUT_OPERAND_BIT                       0
#define NUMPADS_SINGLE                        0u
#define NUMPADS_DUAL                          1u
#define NUMPADS_QUAD                          2u
#define MAX_LUT_ENTRIES                       128                     // Maximum number of 32-bit entries in the LUT.
#define MAX_LUT_SEQUENCES                     (MAX_LUT_ENTRIES >> 2)  // Maximum number of sequences in the LUT. Each sequence is 16 byte large.
#define MAX_NUM_INST                          8                       // Maximum number of instructions in a sequence. Each instruction is 16 bit large.
#define LUTKEY_KEY                            0x5AF05AF0
#define LUTCR_UNLOCK_BIT                      1
#define LUTCR_LOCK_BIT                        0
#define LUT_INDEX_AHB_ACCESS                  0
#define LUT_INDEX_IP_ACCESS                   1

/*********************************************************************
*
*       Clock controller module
*/
#define CCGR_CG_MASK                          0x3uL
#define CCGR_CG_ON                            0x3uL
#define CCGR6_CG_FLEXSPI_BIT                  10
#define CCGR4_CG_IOMUX_BIT                    4
#define CSCMR1_FLEXSPI_CLK_SEL_PLL3           1uL
#define CSCMR1_FLEXSPI_CLK_SEL_PLL2_PFD2      2uL
#define CSCMR1_FLEXSPI_CLK_SEL_MASK           3uL
#define CSCMR1_FLEXSPI_CLK_SEL_BIT            29
#define CSCMR1_FLEXSPI_PODF_MASK              0x7uL
#define CSCMR1_FLEXSPI_PODF_BIT               23

/*********************************************************************
*
*       Peripheral configuration registers
*/
#define MCR0_AHBGRANTWAIT_BIT                 24
#define MCR0_PGRANTWAIT_BIT                   16
#define MCR1_SEQWAIT_BIT                      16
#define MCR1_AHBBUSWAIT_BIT                   0
#define MCR2_RESUMEWAIT_BIT                   24
#define MCR2_SAMEDEVICEEN_BIT                 15
#define AHBCR_PREFETCHEN_BIT                  5
#define AHBCR_READADDROPT_BIT                 6

/*********************************************************************
*
*       Flash configuration registers
*/
#define FLSHCR0_FLSHSZ_BIT                    0
#define FLSHCR1_CSINTERVAL_BIT                16
#define FLSHCR1_TCSH_BIT                      5
#define FLSHCR1_TCSS_BIT                      0
#define FLSHCR2_ARDSEQID_BIT                  0
#define FLSHCR2_ARDSEQID_MASK                 0xFuL
#define FLSHCR2_CLRINSTRPTR_BIT               31

/*********************************************************************
*
*       Command configuration and execution
*/
#define IPCR1_DATSZ_MASK                      0xFFFFuL
#define IPCR1_DATSZ_BIT                       0
#define IPCR1_ISEQID_BIT                      16
#define IPCMD_TRG_BIT                         0

/*********************************************************************
*
*       TX and RX FIFOs
*/
#define IP_FIFO_SIZE                          128     // Number of bytes in a TX and RX FIFOs.
#define FIFO_WATERMARK_RX                     (IP_FIFO_SIZE / 2)
#define FIFO_WATERMARK_TX                     (8)
#define IPRXFCR_RXWMRK_BIT                    2
#define IPTXFCR_TXWMRK_BIT                    2
#define IPRXFSTS_FILL_MASK                    0xFFuL
#define IPRXFSTS_FILL_BIT                     0
#define IPRXFCR_CLRIPRXF_BIT                  0
#define IPTXFCR_CLRIPTXF_BIT                  0

/*********************************************************************
*
*       Status registers
*/
#define INTR_IPCMDDONE_BIT                    0
#define INTR_IPCMDGE_BIT                      1
#define INTR_AHBCMDGE_BIT                     2
#define INTR_IPCMDERR_BIT                     3
#define INTR_AHBCMDERR_BIT                    4
#define INTR_IPRXWA_BIT                       5
#define INTR_IPTXWE_BIT                       6
#define INTR_AHBBUSTIMEOUT_BIT                10
#define INTR_SEQTIMEOUT_BIT                   11
#define STS0_ARBIDLE_BIT                      1
#define STS0_SEQIDLE_BIT                      0

/*********************************************************************
*
*       Misc. defines
*/
#define MEM_SIZE_SHIFT                        28                // Size of the memory region reserved for QUADSPI (256 MB)
#define AHBRXBUFCR_BUFSIZ_BIT                 0
#define AHBRXBUFCR_MSTRID_BIT                 16
#define AHBRXBUFCR_PREFETCHEN_BIT             31
#define FLSHCR4_WMOPT1_BIT                    0

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

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
      if ((MPU_RBAR & ~Mask) == FS_NOR_HW_SPIFI_FLEXSPI_MEM_ADDR) {
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
      MPU_RBAR  = FS_NOR_HW_SPIFI_FLEXSPI_MEM_ADDR;
      MPU_RASR  = 0
                | (1uL                  << RASR_XN_BIT)
                | (RASR_AP_FULL         << RASR_AP_BIT)
                | (1uL                  << RASR_TEX_BIT)
                | ((MEM_SIZE_SHIFT - 1) << RASR_SIZE_BIT)
                | (1uL                  << RASR_ENABLE_BIT)
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
*       _Reset
*/
static void _Reset(void) {
  int            i;
  volatile U32 * pLUT;

  //
  // Clear errors.
  //
  FLEXSPI_INTR = 0
               | (1uL << INTR_IPCMDGE_BIT)
               | (1uL << INTR_AHBCMDGE_BIT)
               | (1uL << INTR_IPCMDERR_BIT)
               | (1uL << INTR_AHBCMDERR_BIT)
               | (1uL << INTR_AHBBUSTIMEOUT_BIT)
               | (1uL << INTR_SEQTIMEOUT_BIT)
               ;
  //
  // Reset FIFOs.
  //
  FLEXSPI_IPTXFCR |= 1uL << IPTXFCR_CLRIPTXF_BIT;
  FLEXSPI_IPRXFCR |= 1uL << IPRXFCR_CLRIPRXF_BIT;
  //
  // Reset the peripheral.
  //
  FLEXSPI_MCR0 |= 1uL << MCR0_SWRESET_BIT;
  for (;;) {
    if ((FLEXSPI_MCR0 & (1uL << MCR0_SWRESET_BIT)) == 0) {
      break;
    }
  }
  //
  // Clear the command LUT.
  //
  FLEXSPI_LUTKEY = LUTKEY_KEY;
  FLEXSPI_LUTCR  = 1uL << LUTCR_UNLOCK_BIT;
  pLUT = &FLEXSPI_LUT;
  for (i = 0; i < MAX_LUT_ENTRIES; ++i) {
    *pLUT++ = 0;
  }
  FLEXSPI_LUTKEY = LUTKEY_KEY;
  FLEXSPI_LUTCR  = 1uL << LUTCR_LOCK_BIT;
}

/*********************************************************************
*
*       _EncodeCmd
*/
static U16 _EncodeCmd(U8 Cmd, U16 BusWidth) {
  U16 Inst;
  U16 NumPads;

  BusWidth = FS_BUSWIDTH_GET_CMD(BusWidth);
  switch (BusWidth) {
  default:
    // through
  case 1:
    NumPads = NUMPADS_SINGLE;
    break;
  case 2:
    NumPads = NUMPADS_DUAL;
    break;
  case 4:
    NumPads = NUMPADS_QUAD;
    break;
  }
  Inst = 0
       | (0x1u         << LUT_OPCODE_BIT)
       | (U16)(NumPads << LUT_NUMPADS_BIT)
       | (U16)(Cmd     << LUT_OPERAND_BIT)
       ;
  return Inst;
}

/*********************************************************************
*
*       _EncodeAddr
*/
static U16 _EncodeAddr(unsigned NumBytes, U16 BusWidth) {
  U16 Inst;
  U16 NumPads;
  U16 NumBits;

  BusWidth = FS_BUSWIDTH_GET_ADDR(BusWidth);
  switch (BusWidth) {
  default:
    // through
  case 1:
    NumPads = NUMPADS_SINGLE;
    break;
  case 2:
    NumPads = NUMPADS_DUAL;
    break;
  case 4:
    NumPads = NUMPADS_QUAD;
    break;
  }
  NumBits = (U16)(NumBytes << 3);
  Inst = 0
       | (0x2u         << LUT_OPCODE_BIT)
       | (U16)(NumPads << LUT_NUMPADS_BIT)
       | (U16)(NumBits << LUT_OPERAND_BIT)
       ;
  return Inst;
}

/*********************************************************************
*
*       _EncodeDummy
*/
static U16 _EncodeDummy(unsigned NumBytes, U16 BusWidth) {
  U16 Inst;
  U16 NumPads;
  U16 NumCycles;

  BusWidth  = FS_BUSWIDTH_GET_ADDR(BusWidth);
  switch (BusWidth) {
  default:
    // through
  case 1:
    NumPads   = NUMPADS_SINGLE;
    NumCycles = NumBytes << 3;
    break;
  case 2:
    NumPads   = NUMPADS_DUAL;
    NumCycles = NumBytes << 2;
    break;
  case 4:
    NumPads   = NUMPADS_QUAD;
    NumCycles = NumBytes << 1;
    break;
  }
  Inst = 0
       | (0xCu           << LUT_OPCODE_BIT)
       | (U16)(NumPads   << LUT_NUMPADS_BIT)
       | (U16)(NumCycles << LUT_OPERAND_BIT)
       ;
  return Inst;
}

/*********************************************************************
*
*       _EncodeRead
*/
static U16 _EncodeRead(U16 BusWidth) {
  U16 Inst;
  U16 NumPads;

  BusWidth = FS_BUSWIDTH_GET_DATA(BusWidth);
  switch (BusWidth) {
  default:
    // through
  case 1:
    NumPads = NUMPADS_SINGLE;
    break;
  case 2:
    NumPads = NUMPADS_DUAL;
    break;
  case 4:
    NumPads = NUMPADS_QUAD;
    break;
  }
  Inst = 0
       | (0x9u         << LUT_OPCODE_BIT)
       | (U16)(NumPads << LUT_NUMPADS_BIT)
       | (1u           << LUT_OPERAND_BIT)     // This value needs to be different than 0. The number of bytes is specified via DATSZ field in FLEXSPI_IPCR1 register.
       ;
  return Inst;
}

/*********************************************************************
*
*       _EncodeWrite
*/
static U16 _EncodeWrite(U16 BusWidth) {
  U16 Inst;
  U16 NumPads;

  BusWidth = FS_BUSWIDTH_GET_DATA(BusWidth);
  switch (BusWidth) {
  default:
    // through
  case 1:
    NumPads = NUMPADS_SINGLE;
    break;
  case 2:
    NumPads = NUMPADS_DUAL;
    break;
  case 4:
    NumPads = NUMPADS_QUAD;
    break;
  }
  Inst = 0
       | (0x8u         << LUT_OPCODE_BIT)
       | (U16)(NumPads << LUT_NUMPADS_BIT)
       | (1u           << LUT_OPERAND_BIT)     // This value needs to be different than 0. The number of bytes is specified via DATSZ field in FLEXSPI_IPCR1 register.
       ;
  return Inst;
}

/*********************************************************************
*
*       _StoreLUTEntry
*/
static int _StoreLUTEntry(unsigned Index, const U16 * pInst, int NumInst) {
  volatile U32 * pLUT;
  U32            Entry;
  int            NumInstRem;
  unsigned       NumItems;

  if (Index > MAX_LUT_SEQUENCES) {
    return 1;           // Error, invalid LUT index.
  }
  if (NumInst > MAX_NUM_INST) {
    return 1;           // Error, invalid number of instructions.
  }
  NumInstRem = MAX_NUM_INST;
  //
  // Unlock access to LUT.
  //
  FLEXSPI_LUTKEY = LUTKEY_KEY;
  FLEXSPI_LUTCR  = 1uL << LUTCR_UNLOCK_BIT;
  pLUT = &FLEXSPI_LUT + (Index << 2);     // Each sequence is 16 bytes large.
  for (;;) {
    if (NumInst == 0) {
      break;
    }
    Entry = (U32)*pInst++;
    if (--NumInst == 0) {
      NumInstRem -= 2;
      *pLUT++ = Entry;
      break;
    }
    Entry |= ((U32)*pInst++ << 16);
    *pLUT = Entry;
    ++pLUT;
    --NumInst;
    NumInstRem -= 2;
  }
  //
  // Fill the rest of the sequence with 0's (STOP instruction.)
  // Otherwise the FLEXSPI controller reports an execution error.
  //
  NumItems = (unsigned)NumInstRem >> 1;   // we store 2 instruction in a 32-bit LUT entry.
  if (NumItems) {
    do {
      *pLUT++ = 0;
    } while (--NumItems);
  }
  //
  // Lock access to LUT.
  //
  FLEXSPI_LUTKEY = LUTKEY_KEY;
  FLEXSPI_LUTCR  = 1uL << LUTCR_LOCK_BIT;
  return 0;             // OK, entries stored.
}

/*********************************************************************
*
*       _WaitForRxReady
*/
static int _WaitForRxReady(unsigned NumBytes) {
  int r;
  U32 Status;
  U32 NumBytesFIFO;

  r = 1;              // Set to indicate an error.
  for (;;) {
    Status = FLEXSPI_INTR;
    if (Status & (1uL << INTR_IPCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_IPCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBBUSTIMEOUT_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_SEQTIMEOUT_BIT)) {
      break;          // Error
    }
    if (NumBytes >= FIFO_WATERMARK_RX) {
      //
      // Quit when there are more bytes in the FIFO as the configured threshold.
      //
      if (Status & (1uL << INTR_IPRXWA_BIT)) {
        r = 0;        // OK, data can be read from FIFO.
        break;
      }
    } else {
      //
      // Quit as soon as all the data is received and stored in the RX FIFO.
      // The FILL field of IPRXFSTS stores the number of 64-bit entries that
      // store data.
      //
      NumBytesFIFO = ((FLEXSPI_IPRXFSTS >> IPRXFSTS_FILL_BIT) & IPRXFSTS_FILL_MASK) << 3;
      if (NumBytesFIFO >= NumBytes) {
        r = 0;        // OK, data can be read from FIFO.
        break;
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _WaitForTxReady
*/
static int _WaitForTxReady(void) {
  int r;
  U32 Status;

  r = 1;              // Set to indicate an error.
  for (;;) {
    Status = FLEXSPI_INTR;
    if (Status & (1uL << INTR_IPCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_IPCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBBUSTIMEOUT_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_SEQTIMEOUT_BIT)) {
      break;          // Error
    }
    //
    // Quit when there are less bytes in the TX FIFO as the configured threshold.
    //
    if (Status & (1uL << INTR_IPTXWE_BIT)) {
      r = 0;        // OK, data can be written to FIFO.
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*       _WaitForIdle
*/
static int _WaitForIdle(void) {
  int r;
  U32 Status;
  U32 Status0;

  r = 1;              // Set to indicate an error.
  for (;;) {
    Status  = FLEXSPI_INTR;
    Status0 = FLEXSPI_STS0;
    if (Status & (1uL << INTR_IPCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDGE_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_IPCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBCMDERR_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_AHBBUSTIMEOUT_BIT)) {
      break;          // Error
    }
    if (Status & (1uL << INTR_SEQTIMEOUT_BIT)) {
      break;          // Error
    }
    if (Status0 & (1uL << STS0_ARBIDLE_BIT)) {
      if (Status0 & (1uL << STS0_SEQIDLE_BIT)) {
        r = 0;          // OK, command executed -> ready for a new command
        break;
      }
    }
  }
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
  FS_USE_PARA(Unit);              // We support only the fist FlexSPI peripheral.
  //
  // Configure and enable the clock of used peripherals.
  //
  if ((CCM_CCGR4 & (CCGR_CG_MASK << CCGR4_CG_IOMUX_BIT)) == 0) {
    CCM_CCGR4 |= CCGR_CG_ON << CCGR4_CG_IOMUX_BIT;
  }
  //
  // Keep in sync this clock configuration with the value set for FS_NOR_HW_SPIFI_NOR_CLK_HZ at the beginning of the file.
  //
  CCM_CCGR6  &= ~(CCGR_CG_MASK << CCGR6_CG_FLEXSPI_BIT);
  CCM_CSCMR1 &= ~((CSCMR1_FLEXSPI_CLK_SEL_MASK << CSCMR1_FLEXSPI_CLK_SEL_BIT) |
                  (CSCMR1_FLEXSPI_PODF_MASK    << CSCMR1_FLEXSPI_PODF_BIT));
  CCM_CSCMR1 |= 0
             | (CSCMR1_FLEXSPI_CLK_SEL_PLL2_PFD2 << CSCMR1_FLEXSPI_CLK_SEL_BIT)
             | (7uL                              << CSCMR1_FLEXSPI_PODF_BIT)
             ;
  CCM_CCGR6  |= CCGR_CG_ON << CCGR6_CG_FLEXSPI_BIT;
  //
  // Disable the cache on the memory region assigned to QSPI
  // so that actual data is visible to the CPU after the write
  // operation.
  //
  _DisableDCache();
  //
  // Configure the Chip Select signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_06 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_06 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Configure the Clock signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_07 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_07 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Configure the Data 0 signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_08 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_08 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Configure the Data 1 signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_09 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_09 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Configure the Data 2 signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_10 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_10 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Configure the Data 3 signal.
  //
  IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B1_11 = SW_MUX_CTL_MUX_MODE_ALT1 << SW_MUX_CTL_MUX_MODE_BIT;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B1_11 = 0
                                      | (1uL                     << SW_PAD_CTL_SRE_BIT)
                                      | (6uL                     << SW_PAD_CTL_DSE_BIT)
                                      | (1uL                     << SW_PAD_CTL_PKE_BIT)
                                      | (SW_PAD_CTL_SPEED_MEDIUM << SW_PAD_CTL_SPEED_BIT)
                                      ;
  //
  // Initialize the Quad-SPI controller.
  //
  FLEXSPI_MCR0          = 1uL << MCR0_MDIS_BIT;
  FLEXSPI_MCR0         |= 0                                   // Set to default.
                       | (0xFFuL   << MCR0_PGRANTWAIT_BIT)
                       | (0xFFuL   << MCR0_AHBGRANTWAIT_BIT)
                       | (1uL      << MCR0_DOSEEN_BIT)
                       ;
  FLEXSPI_MCR1         = 0                                    // Set to default.
                       | (0xFFFFuL << MCR1_SEQWAIT_BIT)
                       | (0xFFFFuL << MCR1_AHBBUSWAIT_BIT)
                       ;
  FLEXSPI_MCR2         = 0                                    // Set to default.
                       | (0x20uL   << MCR2_RESUMEWAIT_BIT)
                       | (1uL      << MCR2_SAMEDEVICEEN_BIT)
                       ;
  FLEXSPI_AHBCR        = 0
                       | (1uL      << AHBCR_PREFETCHEN_BIT)
                       | (1uL      << AHBCR_READADDROPT_BIT)  // This option needs to be enabled so that the CPU reads correct data after a write or erase operation.
                       ;
  FLEXSPI_AHBRXBUF0CR0 = 0;                                   // Use buffer 3 for all the data transfers.
  FLEXSPI_AHBRXBUF1CR0 = 0;
  FLEXSPI_AHBRXBUF2CR0 = 0;
  FLEXSPI_AHBRXBUF3CR0 = 0                                    // Use the same settings as the ROM bootloader.
                       | (32uL << AHBRXBUFCR_BUFSIZ_BIT)
                       | (3uL  << AHBRXBUFCR_MSTRID_BIT)
                       | (1uL  << AHBRXBUFCR_PREFETCHEN_BIT)
                       ;
  FLEXSPI_IPRXFCR      = ((FIFO_WATERMARK_RX / 8) - 1) << IPRXFCR_RXWMRK_BIT;
  FLEXSPI_IPTXFCR      = ((FIFO_WATERMARK_TX / 8) - 1) << IPTXFCR_TXWMRK_BIT;
  FLEXSPI_FLSHA1CR0    = (1uL << (MEM_SIZE_SHIFT - 10)) << FLSHCR0_FLSHSZ_BIT;
  FLEXSPI_FLSHA1CR1    = 0                                    // Use the same settings as per an NXP sample.
                       | (2uL << FLSHCR1_CSINTERVAL_BIT)
                       | (3uL << FLSHCR1_TCSH_BIT)
                       | (3uL << FLSHCR1_TCSS_BIT)
                       ;
  FLEXSPI_FLSHA1CR2    = 0
                       | (LUT_INDEX_AHB_ACCESS << FLSHCR2_ARDSEQID_BIT)
                       ;
  FLEXSPI_FLSHA2CR0    = 0;                                   // Only one NOR flash device is connected.
  FLEXSPI_FLSHA2CR1    = 0;
  FLEXSPI_FLSHA2CR2    = 0;
  FLEXSPI_FLSHB1CR0    = 0;
  FLEXSPI_FLSHB1CR1    = 0;
  FLEXSPI_FLSHB1CR2    = 0;
  FLEXSPI_FLSHB2CR0    = 0;
  FLEXSPI_FLSHB2CR1    = 0;
  FLEXSPI_FLSHB2CR2    = 0;
  FLEXSPI_FLSHCR4      = 0
                       | (1uL << FLSHCR4_WMOPT1_BIT)          // This is required so that FlexSPI is able to perform write operations with 1 byte of data.
                       ;
  FLEXSPI_MCR0        &= ~(1uL << MCR0_MDIS_BIT);
  _Reset();
  return (int)FS_NOR_HW_SPIFI_NOR_CLK_HZ;
}

/*********************************************************************
*
*       _HW_SetCmdMode
*
*  Function description
*    Configures the hardware for direct access to serial NOR flash.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_SetCmdMode(U8 Unit) {
  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  //
  // This function is not required since the hardware can process
  // in parallel request in command and memory mode.
  //
}

/*********************************************************************
*
*       _HW_SetMemMode
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
static void _HW_SetMemMode(U8 Unit, U8 ReadCmd, unsigned NumBytesAddr, unsigned NumBytesDummy, U16 BusWidth) {
  int r;
  U16 aInst[MAX_NUM_INST];
  int NumInst;

  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  //
  // Clear the AHB receive buffer by resetting FlexSPI.
  //
  FLEXSPI_MCR0 |= 1uL << MCR0_SWRESET_BIT;
  for (;;) {
    if ((FLEXSPI_MCR0 & (1uL << MCR0_SWRESET_BIT)) == 0) {
      break;
    }
  }
  //
  // Prepare the read command.
  //
  NumInst = 0;
  FS_MEMSET(aInst, 0, sizeof(aInst));
  aInst[NumInst++] = _EncodeCmd(ReadCmd, BusWidth);
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth);
  }
  if (NumBytesDummy) {
    aInst[NumInst++] = _EncodeDummy(NumBytesDummy, BusWidth);
  }
  aInst[NumInst++] = _EncodeRead(BusWidth);
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_AHB_ACCESS, aInst, NumInst);
  FLEXSPI_FLSHA1CR2 &= ~(FLSHCR2_ARDSEQID_MASK << FLSHCR2_ARDSEQID_BIT);
  FLEXSPI_FLSHA1CR2 |=   LUT_INDEX_AHB_ACCESS  << FLSHCR2_ARDSEQID_BIT;
  //
  // Wait for the access to be granted to AHB.
  //
  r = _WaitForIdle();
  if (r) {
    _Reset();
  }
}

/*********************************************************************
*
*       _HW_ExecCmd
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*
*  Parameters
*    Unit           Index of the hardware layer (0-based).
*    Cmd            Code of the command to be sent.
*    BusWidth       Number of data lines to be used for sending the command.
*/
static void _HW_ExecCmd(U8 Unit, U8 Cmd, U8 BusWidth) {
  U16 aInst[MAX_NUM_INST];
  int NumInst;
  int r;

  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  NumInst = 0;
  //
  // Encode instructions.
  //
  FS_MEMSET(aInst, 0, sizeof(aInst));
  aInst[NumInst++] = _EncodeCmd(Cmd, BusWidth);
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_IP_ACCESS, aInst, NumInst);
  //
  // Prepare the data transfer.
  //
  FLEXSPI_FLSHA1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
  FLEXSPI_IPTXFCR   |= 1uL << IPTXFCR_CLRIPTXF_BIT;
  FLEXSPI_IPRXFCR   |= 1uL << IPRXFCR_CLRIPRXF_BIT;
  FLEXSPI_INTR       = 0
                     | (1uL << INTR_IPCMDGE_BIT)
                     | (1uL << INTR_AHBCMDGE_BIT)
                     | (1uL << INTR_IPCMDERR_BIT)
                     | (1uL << INTR_AHBCMDERR_BIT)
                     | (1uL << INTR_AHBBUSTIMEOUT_BIT)
                     | (1uL << INTR_SEQTIMEOUT_BIT)
                     ;
  FLEXSPI_IPCR1      = 0
                     | (LUT_INDEX_IP_ACCESS << IPCR1_ISEQID_BIT)
                     ;
  //
  // Start command execution.
  //
  FLEXSPI_IPCMD      = 1uL << IPCMD_TRG_BIT;
  r = _WaitForIdle();
  if (r) {
    _Reset();
  }
}

/*********************************************************************
*
*       _HW_ReadData
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
static void _HW_ReadData(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  U16            aInst[MAX_NUM_INST];
  int            NumInst;
  U32            NumBytes;
  U32            AddrReg;
  int            r;
  unsigned       NumItems;
  volatile U32 * pDataReg;
  U32          * pData32;
  U32            Data32;

  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  NumInst = 0;
  AddrReg = 0;
  //
  // Encode instructions.
  //
  FS_MEMSET(aInst, 0, sizeof(aInst));
  aInst[NumInst++] = _EncodeCmd(Cmd, BusWidth);
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth);
  }
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    aInst[NumInst++] = _EncodeDummy(NumBytes, BusWidth);
  }
  FLEXSPI_IPCR1 &= ~(IPCR1_DATSZ_MASK << IPCR1_DATSZ_BIT);
  if (NumBytesData) {
    aInst[NumInst++] = _EncodeRead(BusWidth);
  }
  if (NumBytesAddr) {
    NumBytes = NumBytesAddr;
    do {
      AddrReg <<= 8;
      AddrReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_IP_ACCESS, aInst, NumInst);
  //
  // Prepare the data transfer.
  //
  FLEXSPI_FLSHA1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
  FLEXSPI_IPTXFCR   |= 1uL << IPTXFCR_CLRIPTXF_BIT;
  FLEXSPI_IPRXFCR   |= 1uL << IPRXFCR_CLRIPRXF_BIT;
  FLEXSPI_INTR       = 0
                     | (1uL << INTR_IPCMDGE_BIT)
                     | (1uL << INTR_AHBCMDGE_BIT)
                     | (1uL << INTR_IPCMDERR_BIT)
                     | (1uL << INTR_AHBCMDERR_BIT)
                     | (1uL << INTR_AHBBUSTIMEOUT_BIT)
                     | (1uL << INTR_SEQTIMEOUT_BIT)
                     ;
  FLEXSPI_IPCR0      = AddrReg;
  FLEXSPI_IPCR1      = 0
                     | (NumBytesData        << IPCR1_DATSZ_BIT)
                     | (LUT_INDEX_IP_ACCESS << IPCR1_ISEQID_BIT)
                     ;
  //
  // Start command execution.
  //
  FLEXSPI_IPCMD      = 1uL << IPCMD_TRG_BIT;
  //
  // Receive data from NOR flash device.
  //
  for (;;) {
    if (NumBytesData == 0) {
      break;                              // OK, all the data has been read.
    }
    r = _WaitForRxReady(NumBytesData);
    if (r) {
      _Reset();
      break;                              // Error
    }
    pDataReg = &FLEXSPI_RFDR0;
    if (NumBytesData < FIFO_WATERMARK_RX) {
      NumItems = NumBytesData >> 2;       // Read 4 bytes at a time.
      if (NumItems) {
        NumBytesData -= NumItems << 2;
        if (((U32)pData & 3) == 0) {
          //
          // 32-bit aligned destination buffer.
          //
          pData32  = (U32 *)(void *)pData;
          pData   += NumItems << 2;
          do {
            *pData32++ = *pDataReg++;
          } while (--NumItems);
        } else {
          //
          // Destination buffer is not 32-bit aligned.
          //
          do {
            Data32 =  *pDataReg++;
            *pData++ = (U8)Data32;
            *pData++ = (U8)(Data32 >> 8);
            *pData++ = (U8)(Data32 >> 16);
            *pData++ = (U8)(Data32 >> 24);
          } while (--NumItems);
        }
      }
      if (NumBytesData) {
        Data32 = *pDataReg;
        if (NumBytesData == 1) {
          *pData++ = (U8)Data32;
        } else if (NumBytesData == 2) {
          *pData++ = (U8)Data32;
          *pData++ = (U8)(Data32 >> 8);
        } else if (NumBytesData == 3) {
          *pData++ = (U8)Data32;
          *pData++ = (U8)(Data32 >> 8);
          *pData++ = (U8)(Data32 >> 16);
        }
        NumBytesData = 0;
      }
    } else {
      NumItems = FIFO_WATERMARK_RX >> 2;      // Read 32-bit items at a time.
      if (NumItems) {
        NumBytesData -= NumItems << 2;
        if (((U32)pData & 3) == 0) {
          //
          // 32-bit aligned destination buffer.
          //
          pData32  = (U32 *)(void *)pData;
          pData   += NumItems << 2;
          do {
            *pData32++ = *pDataReg++;
          } while (--NumItems);
        } else {
          //
          // Destination buffer is not 32-bit aligned.
          //
          do {
            Data32 =  *pDataReg++;
            *pData++ = (U8)Data32;
            *pData++ = (U8)(Data32 >> 8);
            *pData++ = (U8)(Data32 >> 16);
            *pData++ = (U8)(Data32 >> 24);
          } while (--NumItems);
        }
      }
    }
    //
    // Tell the peripheral that we read a number of bytes equal to the RX FIFO threshold.
    //
    FLEXSPI_INTR |= 1uL << INTR_IPRXWA_BIT;
  }
  r = _WaitForIdle();
  if (r) {
    _Reset();
  }
}

/*********************************************************************
*
*       _HW_WriteData
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
static void _HW_WriteData(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  U16            aInst[MAX_NUM_INST];
  int            NumInst;
  U32            NumBytes;
  U32            AddrReg;
  int            r;
  unsigned       NumItems;
  volatile U32 * pDataReg;
  const U32    * pData32;
  U32            Data32;

  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  NumInst = 0;
  AddrReg = 0;
  //
  // Encode instructions.
  //
  FS_MEMSET(aInst, 0, sizeof(aInst));
  aInst[NumInst++] = _EncodeCmd(Cmd, BusWidth);
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth);
  }
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    aInst[NumInst++] = _EncodeDummy(NumBytes, BusWidth);
  }
  FLEXSPI_IPCR1 &= ~(IPCR1_DATSZ_MASK << IPCR1_DATSZ_BIT);
  if (NumBytesData) {
    aInst[NumInst++] = _EncodeWrite(BusWidth);
  }
  if (NumBytesAddr) {
    NumBytes = NumBytesAddr;
    do {
      AddrReg <<= 8;
      AddrReg  |= (U32)(*pPara++);
    } while (--NumBytes);
  }
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_IP_ACCESS, aInst, NumInst);
  //
  // Prepare the data transfer.
  //
  FLEXSPI_FLSHA1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
  FLEXSPI_IPTXFCR   |= 1uL << IPTXFCR_CLRIPTXF_BIT;
  FLEXSPI_IPRXFCR   |= 1uL << IPRXFCR_CLRIPRXF_BIT;
  FLEXSPI_INTR       = 0
                     | (1uL << INTR_IPCMDGE_BIT)
                     | (1uL << INTR_AHBCMDGE_BIT)
                     | (1uL << INTR_IPCMDERR_BIT)
                     | (1uL << INTR_AHBCMDERR_BIT)
                     | (1uL << INTR_AHBBUSTIMEOUT_BIT)
                     | (1uL << INTR_SEQTIMEOUT_BIT)
                     ;
  FLEXSPI_IPCR0      = AddrReg;
  FLEXSPI_IPCR1      = 0
                     | (NumBytesData        << IPCR1_DATSZ_BIT)
                     | (LUT_INDEX_IP_ACCESS << IPCR1_ISEQID_BIT)
                     ;
  //
  // Start command execution.
  //
  FLEXSPI_IPCMD     |= 1uL << IPCMD_TRG_BIT;
  //
  // Send data to NOR flash device.
  //
  for (;;) {
    if (NumBytesData == 0) {
      break;                              // OK, all the data has been written.
    }
    r = _WaitForTxReady();
    if (r) {
      _Reset();
      break;                              // Error
    }
    pDataReg = &FLEXSPI_TFDR0;
    if (NumBytesData < FIFO_WATERMARK_TX) {
      NumItems = NumBytesData >> 2;       // Write 4 bytes at a time.
      if (NumItems) {
        NumBytesData -= NumItems << 2;
        if (((U32)pData & 3) == 0) {
          //
          // 32-bit aligned source buffer.
          //
          pData32  = (U32 *)(void *)pData;
          pData   += NumItems << 2;
          do {
            *pDataReg++ = *pData32++;
          } while (--NumItems);
        } else {
          //
          // Source buffer is not 32-bit aligned.
          //
          do {
            Data32  = (U32)(*pData++);
            Data32 |= (U32)(*pData++) << 8;
            Data32 |= (U32)(*pData++) << 16;
            Data32 |= (U32)(*pData++) << 24;
            *pDataReg++ = Data32;
          } while (--NumItems);
        }
      }
      if (NumBytesData) {
        Data32 = 0;
        if (NumBytesData == 1) {
          Data32 = (U32)(*pData++);
        } else if (NumBytesData == 2) {
          Data32  = (U32)(*pData++);
          Data32 |= (U32)(*pData++ << 8);
        } else if (NumBytesData == 3) {
          Data32  = (U32)(*pData++);
          Data32 |= (U32)(*pData++ << 8);
          Data32 |= (U32)(*pData++ << 16);
        }
        *pDataReg = Data32;
        NumBytesData = 0;
      }
    } else {
      NumItems = FIFO_WATERMARK_TX >> 2;      // Write 32-bit items at a time.
      if (NumItems == 0) {
        NumBytesData = 0;                     // Error, invalid FIFO watermark.
      } else {
        NumBytesData -= NumItems << 2;
        if (((U32)pData & 3) == 0) {
          //
          // 32-bit aligned source buffer.
          //
          pData32  = (U32 *)(void *)pData;
          pData   += NumItems << 2;
          do {
            *pDataReg++ = *pData32++;
          } while (--NumItems);
        } else {
          //
          // Source buffer is not 32-bit aligned.
          //
          do {
            Data32  = (U32)(*pData++);
            Data32 |= (U32)(*pData++) << 8;
            Data32 |= (U32)(*pData++) << 16;
            Data32 |= (U32)(*pData++) << 24;
            *pDataReg++ = Data32;
          } while (--NumItems);
        }
      }
    }
    //
    // Tell the peripheral that we have written a number of bytes equal to the TX FIFO threshold.
    // First, make sure that the data was written to the hardware.
    //
    __asm("DSB");
    __asm("ISB");
    FLEXSPI_INTR |= 1uL << INTR_IPTXWE_BIT;
  }
  r = _WaitForIdle();
  if (r) {
    _Reset();
  }
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_iMXRT1052_NXP_MIMXRT1050_EVK = {
  _HW_Init,
  _HW_SetCmdMode,
  _HW_SetMemMode,
  _HW_ExecCmd,
  _HW_ReadData,
  _HW_WriteData,
  NULL,
  NULL,
  NULL,
  NULL
#if (FS_VERSION >= 51800u)
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
#endif
};

/*************************** End of file ****************************/
