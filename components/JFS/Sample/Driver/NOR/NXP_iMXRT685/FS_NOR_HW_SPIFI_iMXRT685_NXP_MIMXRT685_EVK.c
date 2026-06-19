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

File    : FS_NOR_HW_SPIFI_iMXRT685_NXP_MIMXRT685_EVK.c
Purpose : Low-level flash driver for NXP iMXRT685 FlexSPI interface.
Literature:
  [1] UM11147 RT6xx User manual
    (\\fileserver\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT600x\iMXRT600_B0\RT600_UserManual_Rev1.4.pdf)
  [2] RT600 Product Data Sheet
    (\\fileserver\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT600x\iMXRT600_B0\RT600_Datasheet_Rev2.0.pdf)
  [3] Errata sheet RT600
    (\\fileserver\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT600x\iMXRT600_B0\RT600_Errata_Rev1.9.pdf)
  [4] Schematics IMXRT685-EVK
    (\\fileserver\Techinfo\Company\NXP\MCU\iMX(ARM)\iMXRT600x\Evalboard\SPF-35099_E2_210119.pdf)
  [5] Datasheet Macronix MX25UM51345G
    ("\\fileserver\Techinfo\Company\Macronix\SPI_NOR_Flash\MX25UM51345G, 1.8V, 512Mb, v1.3.pdf")

Additional information:

  Pin name      GPIO            Alt. function
  -------------------------------------------
  QSPI_B_CS0    PIO2_19         ALT6
  QSPI_B_SCK    PIO1_29         ALT5
  QSPI_B_DQS    not connected   -
  QSPI_B_DATA0  PIO1_11         ALT6
  QSPI_B_DATA1  PIO1_12         ALT6
  QSPI_B_DATA2  PIO1_13         ALT6
  QSPI_B_DATA3  PIO1_14         ALT6
  QSPI_B_DATA4  PIO2_17         ALT6
  QSPI_B_DATA5  PIO2_18         ALT6
  QSPI_B_DATA6  PIO2_22         ALT6
  QSPI_B_DATA7  PIO2_23         ALT6
  nRESET_OSPI   PIO2_12         ALT0

  The DTR mode cannot be used on the evaluation board because the
  FlexSPI port B on which the Macronix NOR flash device is connected
  does not have a DQS signal.
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
#ifndef   FS_NOR_HW_SPIFI_FLEXSPI_CLK_HZ
  #define FS_NOR_HW_SPIFI_FLEXSPI_CLK_HZ          300000000         // Frequency of the clock supplied to FlexSPI unit.
#endif

#ifndef   FS_NOR_HW_SPIFI_NOR_CLK_HZ
  #define FS_NOR_HW_SPIFI_NOR_CLK_HZ              60000000          // Frequency of the clock supplied to NOR flash device.
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
#define FLEXSPI_BASE_ADDR                     0x40134000uL
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
#define FLEXSPI_AHBRXBUF4CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x030))   // AHB RX Buffer 4 Control Register 0
#define FLEXSPI_AHBRXBUF5CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x034))   // AHB RX Buffer 5 Control Register 0
#define FLEXSPI_AHBRXBUF6CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x038))   // AHB RX Buffer 6 Control Register 0
#define FLEXSPI_AHBRXBUF7CR0                  (*(volatile U32 *)(FLEXSPI_BASE_ADDR + 0x03C))   // AHB RX Buffer 7 Control Register 0
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
*       IO pin configuration
*/
#define IOCON_BASE_ADDR                       0x40004000uL
#define PIO1_11                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x0AC))
#define PIO1_12                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x0B0))
#define PIO1_13                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x0B4))
#define PIO1_14                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x0B8))
#define PIO1_29                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x0F4))
#define PIO2_12                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x130))
#define PIO2_17                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x144))
#define PIO2_18                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x148))
#define PIO2_19                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x14C))
#define PIO2_22                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x158))
#define PIO2_23                               (*(volatile U32*)(IOCON_BASE_ADDR + 0x15C))

/*********************************************************************
*
*       GPIO ports
*/
#define GPIO_BASE_ADDR                        0x40100000uL
#define GPIO_DIRSET2                          (*(volatile U32*)(GPIO_BASE_ADDR + 0x2388))   // Port 2 set direction register
#define GPIO_SET2                             (*(volatile U32*)(GPIO_BASE_ADDR + 0x2208))   // Port 2 set register
#define GPIO_CLR2                             (*(volatile U32*)(GPIO_BASE_ADDR + 0x2288))   // Port 2 clear register

/*********************************************************************
*
*       Clock controller 0
*/
#define CLKCTL0_BASE_ADDR                     0x40001000uL
#define CLKCTL0_PSCCTL0                       (*(volatile U32*)(CLKCTL0_BASE_ADDR + 0x10))
#define CLKCTL0_PSCCTL0_SET                   (*(volatile U32*)(CLKCTL0_BASE_ADDR + 0x40))
#define CLKCTL0_PSCCTL0_CLR                   (*(volatile U32*)(CLKCTL0_BASE_ADDR + 0x70))
#define CLKCTL0_FLEXSPIFCLKSEL                (*(volatile U32*)(CLKCTL0_BASE_ADDR + 0x620))
#define CLKCTL0_FLEXSPIFCLKDIV                (*(volatile U32*)(CLKCTL0_BASE_ADDR + 0x624))

/*********************************************************************
*
*       System controller 0
*/
#define SYSCTL0_BASE_ADDR                     0x40002000uL
#define SYSCTL0_AHB_FLEXSPI_ACCESS_DISABLE    (*(volatile U32*)(SYSCTL0_BASE_ADDR + 0x10))

/*********************************************************************
*
*       Reset controller 0
*/
#define RSTCTL0_BASE_ADDR                     0x40000000uL
#define RSTCTL0_PRSTCTL0                      (*(volatile U32*)(RSTCTL0_BASE_ADDR + 0x10))
#define RSTCTL0_PRSTCTL0_SET                  (*(volatile U32*)(RSTCTL0_BASE_ADDR + 0x40))
#define RSTCTL0_PRSTCTL0_CLR                  (*(volatile U32*)(RSTCTL0_BASE_ADDR + 0x70))

/*********************************************************************
*
*       Clock controller 1
*/
#define CLKCTL1_BASE_ADDR                     0x40021000uL
#define CLKCTL1_PSCCTL1                       (*(volatile U32*)(CLKCTL1_BASE_ADDR + 0x14))
#define CLKCTL1_PSCCTL1_SET                   (*(volatile U32*)(CLKCTL1_BASE_ADDR + 0x44))
#define CLKCTL1_PSCCTL1_CLR                   (*(volatile U32*)(CLKCTL1_BASE_ADDR + 0x74))

/*********************************************************************
*
*       Reset controller 1
*/
#define RSTCTL1_BASE_ADDR                     0x40020000uL
#define RSTCTL1_PRSTCTL1                      (*(volatile U32*)(RSTCTL1_BASE_ADDR + 0x14))
#define RSTCTL1_PRSTCTL1_SET                  (*(volatile U32*)(RSTCTL1_BASE_ADDR + 0x44))
#define RSTCTL1_PRSTCTL1_CLR                  (*(volatile U32*)(RSTCTL1_BASE_ADDR + 0x74))

/*********************************************************************
*
*       FlexSPI cache
*/
#define FLEXSPI_CACHE_BASE_ADDR               0x40033000uL
#define FLEXSPI_CACHE_CCR                     (*(volatile U32*)(FLEXSPI_CACHE_BASE_ADDR + 0x800))
#define FLEXSPI_CACHE_CLCR                    (*(volatile U32*)(FLEXSPI_CACHE_BASE_ADDR + 0x804))
#define FLEXSPI_CACHE_CSAR                    (*(volatile U32*)(FLEXSPI_CACHE_BASE_ADDR + 0x808))
#define FLEXSPI_CACHE_CCVR                    (*(volatile U32*)(FLEXSPI_CACHE_BASE_ADDR + 0x80C))

/*********************************************************************
*
*       Port I/O settings
*/
#define PIO_FUNC_BIT                          0
#define PIO_PUPDENA_BIT                       4
#define PIO_PUPDSEL_BIT                       5
#define PIO_IBENA_BIT                         6
#define PIO_SLEWRATE_BIT                      7
#define PIO_FULLDRIVE_BIT                     8
#define PIO_AMENA_BIT                         9
#define PIO_ODENA_BIT                         10
#define PIO_IIENA_BIT                         11

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
#define NUMPADS_OCTAL                         3u
#define MAX_LUT_ENTRIES                       128                     // Maximum number of 32-bit entries in the LUT.
#define MAX_LUT_SEQUENCES                     (MAX_LUT_ENTRIES >> 2)  // Maximum number of sequences in the LUT. Each sequence is 16 byte large.
#define MAX_NUM_INST                          8                       // Maximum number of instructions in a sequence. Each instruction is 16-bit large.
#define LUTKEY_KEY                            0x5AF05AF0
#define LUTCR_UNLOCK_BIT                      1
#define LUTCR_LOCK_BIT                        0
#define LUT_INDEX_AHB_ACCESS                  0
#define LUT_INDEX_IP_ACCESS                   1

/*********************************************************************
*
*       FlexSPI configuration registers
*/
#define MCR0_SWRESET_BIT                      0
#define MCR0_MDIS_BIT                         1
#define MCR0_DOSEEN_BIT                       12
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
#define MEM_SIZE_SHIFT                        28                // Size of the memory region reserved for NOR flash
#define WAIT_TIMEOUT_CYCLES                   100000
#define PSCCTL0_FLEXSPI_OTFAD_CLK_BIT         16
#define PRSTCTL0_FLEXSPI_OTFAD_BIT            16
#define FLEXSPIFCLKSEL_SEL_BIT                0
#define FLEXSPIFCLKSEL_SEL_MASK               0x7uL
#define FLEXSPIFCLKSEL_SEL_MAIN_PLL           1
#define FLEXSPIFCLKDIV_DIV_BIT                0
#define FLEXSPIFCLKDIV_DIV_MASK               0xFFuL
#define AHB_FLEXSPI_ACCESS_DISABLE_BIT        0
#define NOR_RESET_BIT                         12
#define PSCCTL1_HSGPIO2_CLK_BIT               2
#define PRSTCTL1_HSGPIO2_RST_BIT              2
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
*       _Reset
*
*  Function description
*    Performs a reset of FlexSPI and of the NOR flash device.
*/
static void _Reset(void) {
  U32            TimeOut;
  int            i;
  volatile U32 * pLUT;

  //
  // Clear errors.
  //
  FLEXSPI_INTR |= 0
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
  //
  // Reset the NOR flash device. The reset signal is active low.
  //
  GPIO_CLR2 = 1u << NOR_RESET_BIT;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if (--TimeOut == 0) {
      break;
    }
  }
  GPIO_SET2 = 1u << NOR_RESET_BIT;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if (--TimeOut == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       _EncodeCmd
*
*  Function description
*    Creates a LUT sequence that sends a command.
*
*  Additional information
*    BusWith is not an encoded value. It is the actual number of
*    data lines to be used for sending the command.
*/
static U16 _EncodeCmd(unsigned Cmd, unsigned BusWidth, unsigned Flags) {
  unsigned Inst;
  unsigned NumPads;
  unsigned OpCode;

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
  case 8:
    NumPads = NUMPADS_OCTAL;
    break;
  }
  OpCode = 0x01;
  if ((Flags & FS_NOR_HW_FLAG_DTR_CMD) != 0) {
    OpCode = 0x21;
  }
  Inst = 0
       | (OpCode  << LUT_OPCODE_BIT)
       | (NumPads << LUT_NUMPADS_BIT)
       | (Cmd     << LUT_OPERAND_BIT)
       ;
  return (U16)Inst;
}

/*********************************************************************
*
*       _EncodeAddr
*
*  Function description
*    Creates a LUT sequence that sends an address.
*/
static U16 _EncodeAddr(unsigned NumBytes, unsigned BusWidth, unsigned Flags) {
  unsigned Inst;
  unsigned NumPads;
  unsigned NumBits;
  unsigned OpCode;

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
  case 8:
    NumPads = NUMPADS_OCTAL;
    break;
  }
  NumBits = NumBytes << 3;
  OpCode = 0x02;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0) {
    OpCode = 0x22;
  }
  Inst = 0
       | (OpCode  << LUT_OPCODE_BIT)
       | (NumPads << LUT_NUMPADS_BIT)
       | (NumBits << LUT_OPERAND_BIT)
       ;
  return (U16)Inst;
}

/*********************************************************************
*
*       _EncodeDummy
*
*  Function description
*    Creates a LUT sequence that generates dummy cycles.
*/
static U16 _EncodeDummy(unsigned NumBytes, unsigned BusWidth, unsigned Flags) {
  unsigned Inst;
  unsigned NumPads;
  unsigned NumCycles;
  unsigned OpCode;

  BusWidth = FS_BUSWIDTH_GET_ADDR(BusWidth);
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
  case 8:
    NumPads   = NUMPADS_OCTAL;
    NumCycles = NumBytes;
    break;
  }
  OpCode = 0x0C;
  if ((Flags & FS_NOR_HW_FLAG_DTR_ADDR) != 0) {
    OpCode = 0x2C;
  }
  Inst = 0
       | (OpCode    << LUT_OPCODE_BIT)
       | (NumPads   << LUT_NUMPADS_BIT)
       | (NumCycles << LUT_OPERAND_BIT)
       ;
  return (U16)Inst;
}

/*********************************************************************
*
*       _EncodeRead
*
*  Function description
*    Creates a LUT sequence that transfers data from NOR flash device to MCU.
*/
static U16 _EncodeRead(unsigned BusWidth, unsigned Flags) {
  unsigned Inst;
  unsigned NumPads;
  unsigned OpCode;

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
  case 8:
    NumPads = NUMPADS_OCTAL;
    break;
  }
  OpCode = 0x09;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0) {
    OpCode = 0x29;
  }
  Inst = 0
       | (OpCode  << LUT_OPCODE_BIT)
       | (NumPads << LUT_NUMPADS_BIT)
       ;
  return (U16)Inst;
}

/*********************************************************************
*
*       _EncodeWrite
*
*  Function description
*    Creates a LUT sequence that transfers data from MCU to NOR flash device.
*/
static U16 _EncodeWrite(unsigned BusWidth, unsigned Flags) {
  unsigned Inst;
  unsigned NumPads;
  unsigned OpCode;

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
  case 8:
    NumPads = NUMPADS_OCTAL;
    break;
  }
  OpCode = 0x08;
  if ((Flags & FS_NOR_HW_FLAG_DTR_DATA) != 0) {
    OpCode = 0x28;
  }
  Inst = 0
       | (OpCode  << LUT_OPCODE_BIT)
       | (NumPads << LUT_NUMPADS_BIT)
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
    return 1;                             // Error, invalid LUT index.
  }
  if (NumInst > MAX_NUM_INST) {
    return 1;                             // Error, invalid number of instructions.
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
  // Otherwise the FLEXSPI controller will report an execution error.
  //
  NumItems = (unsigned)NumInstRem >> 1;   // We store 2 instructions in a 32-bit LUT entry.
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
  return 0;                               // OK, entries stored.
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
*       _Map
*
*  Function description
*    Configures the hardware for access to serial NOR flash via system memory.
*/
static int _Map(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U16 BusWidth, unsigned Flags) {
  int      r;
  U16      aInst[MAX_NUM_INST];
  int      NumInst;
  unsigned NumBytesDummy;

  FS_USE_PARA(Unit);
  FS_USE_PARA(pPara);
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
  NumInst       = 0;
  NumBytesDummy = NumBytesPara - NumBytesAddr;
  FS_MEMSET(aInst, 0, sizeof(aInst));
  if (NumBytesCmd != 0) {
    do {
      aInst[NumInst++] = _EncodeCmd(*pCmd++, FS_BUSWIDTH_GET_CMD(BusWidth), Flags);
    } while (--NumBytesCmd != 0);
  }
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth, Flags);
  }
  if (NumBytesDummy) {
    aInst[NumInst++] = _EncodeDummy(NumBytesDummy, BusWidth, Flags);
  }
  aInst[NumInst++] = _EncodeRead(BusWidth, Flags);
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_AHB_ACCESS, aInst, NumInst);
  FLEXSPI_FLSHB1CR2 &= ~(FLSHCR2_ARDSEQID_MASK << FLSHCR2_ARDSEQID_BIT);
  FLEXSPI_FLSHB1CR2 |=   LUT_INDEX_AHB_ACCESS  << FLSHCR2_ARDSEQID_BIT;
  //
  // Wait for the access to be granted to AHB.
  //
  r = _WaitForIdle();
  if (r != 0) {
    _Reset();
  }
  return r;
}

/*********************************************************************
*
*       _Control
*
*  Function description
*    Sends a command to serial NOR flash that does not transfer any data.
*/
static int _Control(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, U8 BusWidth, unsigned Flags) {
  U16 aInst[MAX_NUM_INST];
  int NumInst;
  int r;

  FS_USE_PARA(Unit);
  NumInst = 0;
  //
  // Encode instructions.
  //
  FS_MEMSET(aInst, 0, sizeof(aInst));
  if (NumBytesCmd != 0) {
    do {
      aInst[NumInst++] = _EncodeCmd(*pCmd++, BusWidth, Flags);
    } while (--NumBytesCmd != 0);
  }
  //
  // Store the instructions in the internal list of FlexSPI.
  //
  _StoreLUTEntry(LUT_INDEX_IP_ACCESS, aInst, NumInst);
  //
  // Prepare the data transfer.
  //
  FLEXSPI_FLSHB1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
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
  if (r != 0) {
    _Reset();
  }
  return r;
}

/*********************************************************************
*
*       _Read
*
*  Function description
*    HW layer function. It transfers data from NOR flash to MCU.
*/
static int _Read(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
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
  if (NumBytesCmd != 0) {
    do {
      aInst[NumInst++] = _EncodeCmd(*pCmd++, FS_BUSWIDTH_GET_CMD(BusWidth), Flags);
    } while (--NumBytesCmd != 0);
  }
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth, Flags);
  }
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    aInst[NumInst++] = _EncodeDummy(NumBytes, BusWidth, Flags);
  }
  FLEXSPI_IPCR1 &= ~(IPCR1_DATSZ_MASK << IPCR1_DATSZ_BIT);
  if (NumBytesData) {
    aInst[NumInst++] = _EncodeRead(BusWidth, Flags);
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
  FLEXSPI_FLSHB1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
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
    FLEXSPI_INTR = 1uL << INTR_IPRXWA_BIT;
  }
  r = _WaitForIdle();
  if (r != 0) {
    _Reset();
  }
  return r;
}

/*********************************************************************
*
*       _Write
*
*  Function description
*    HW layer function. It transfers data from MCU to NOR flash.
*/
static int _Write(U8 Unit, const U8 * pCmd, unsigned NumBytesCmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth, unsigned Flags) {
  U16            aInst[MAX_NUM_INST];
  int            NumInst;
  U32            NumBytes;
  U32            AddrReg;
  int            r;
  unsigned       NumItems;
  volatile U32 * pDataReg;
  const U32    * pData32;
  U32            Data32;

  FS_USE_PARA(Unit);
  NumInst = 0;
  AddrReg = 0;
  //
  // Encode instructions.
  //
  FS_MEMSET(aInst, 0, sizeof(aInst));
  if (NumBytesCmd != 0) {
    do {
      aInst[NumInst++] = _EncodeCmd(*pCmd++, FS_BUSWIDTH_GET_CMD(BusWidth), Flags);
    } while (--NumBytesCmd != 0);
  }
  if (NumBytesAddr) {
    aInst[NumInst++] = _EncodeAddr(NumBytesAddr, BusWidth, Flags);
  }
  if (NumBytesPara > NumBytesAddr) {
    NumBytes = NumBytesPara - NumBytesAddr;
    aInst[NumInst++] = _EncodeDummy(NumBytes, BusWidth, Flags);
  }
  FLEXSPI_IPCR1 &= ~(IPCR1_DATSZ_MASK << IPCR1_DATSZ_BIT);
  if (NumBytesData) {
    aInst[NumInst++] = _EncodeWrite(BusWidth, Flags);
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
  FLEXSPI_FLSHB1CR2 |= 1uL << FLSHCR2_CLRINSTRPTR_BIT;
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
  if (r != 0) {
    _Reset();
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
  U32 TimeOut;
  U32 ClkDiv;

  FS_USE_PARA(Unit);              // We support only the second FlexSPI peripheral.
  //
  // Configure and enable the clock of used peripherals.
  //
  if ((CLKCTL0_PSCCTL0 & (1uL << PSCCTL0_FLEXSPI_OTFAD_CLK_BIT)) == 0) {
    CLKCTL0_PSCCTL0_SET = 1uL << PSCCTL0_FLEXSPI_OTFAD_CLK_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((CLKCTL0_PSCCTL0 & (1uL << PSCCTL0_FLEXSPI_OTFAD_CLK_BIT)) == 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  CLKCTL0_FLEXSPIFCLKSEL &= ~(FLEXSPIFCLKSEL_SEL_MASK << FLEXSPIFCLKSEL_SEL_BIT);
  CLKCTL0_FLEXSPIFCLKSEL |=   FLEXSPIFCLKSEL_SEL_MAIN_PLL << FLEXSPIFCLKSEL_SEL_BIT;
  CLKCTL0_FLEXSPIFCLKDIV &= ~(FLEXSPIFCLKDIV_DIV_MASK << FLEXSPIFCLKDIV_DIV_BIT);
  ClkDiv = FS_NOR_HW_SPIFI_FLEXSPI_CLK_HZ / FS_NOR_HW_SPIFI_NOR_CLK_HZ - 1u;
  CLKCTL0_FLEXSPIFCLKDIV |=  ClkDiv << FLEXSPIFCLKDIV_DIV_BIT;
  //
  // Make sure that we can access the registers of FlexSPI.
  //
  SYSCTL0_AHB_FLEXSPI_ACCESS_DISABLE &= ~(1uL << AHB_FLEXSPI_ACCESS_DISABLE_BIT);
  //
  // Release FlexSPI from reset.
  //
  if ((RSTCTL0_PRSTCTL0 & (1uL << PRSTCTL0_FLEXSPI_OTFAD_BIT)) == 0) {
    RSTCTL0_PRSTCTL0_CLR = 1uL << PRSTCTL0_FLEXSPI_OTFAD_BIT;
  }
  //
  // Enable the GPIO port used by the reset signal.
  //
  if ((CLKCTL1_PSCCTL1 & (1uL << PSCCTL1_HSGPIO2_CLK_BIT)) == 0) {
    CLKCTL1_PSCCTL1_SET = 1uL << PSCCTL1_HSGPIO2_CLK_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((CLKCTL1_PSCCTL1 & (1uL << PSCCTL1_HSGPIO2_CLK_BIT)) != 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  if ((RSTCTL1_PRSTCTL1 & (1uL << PRSTCTL1_HSGPIO2_RST_BIT)) != 0) {
    RSTCTL1_PRSTCTL1_CLR = 1uL << PRSTCTL1_HSGPIO2_RST_BIT;
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((RSTCTL1_PRSTCTL1 & (1uL << PRSTCTL1_HSGPIO2_RST_BIT)) == 0) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
  //
  // Configure the I/O pins.
  //
  PIO2_19 = 0u                            // Chip Select signal
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO1_29 = 0u                            // Clocks signal
          | (5uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO1_11 = 0u                            // Data 0 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO1_12 = 0u                            // Data 1 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO1_13 = 0u                            // Data 2 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO1_14 = 0u                            // Data 3 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO2_17 = 0u                            // Data 4 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO2_18 = 0u                            // Data 5 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO2_22 = 0u                            // Data 6 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO2_23 = 0u                            // Data 7 signal.
          | (6uL << PIO_FUNC_BIT)
          | (1uL << PIO_IBENA_BIT)
          | (1uL << PIO_FULLDRIVE_BIT)
          ;
  PIO2_12 = 0u                            // Reset signal.
          | (1uL << PIO_IBENA_BIT)
          ;
  GPIO_DIRSET2 = 1uL << NOR_RESET_BIT;
  //
  // Disable the FlexSPI cache.
  //
  FLEXSPI_CACHE_CCR = 0;
  //
  // Initialize the Quad-SPI controller.
  //
  FLEXSPI_MCR0         = 1uL << MCR0_MDIS_BIT;
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
                       ;
  FLEXSPI_AHBCR        = 0
                       | (1uL      << AHBCR_PREFETCHEN_BIT)
                       | (1uL      << AHBCR_READADDROPT_BIT)  // This option needs to be enabled so that the CPU correct reads data after a write or erase operation.
                       ;
  FLEXSPI_AHBRXBUF0CR0 = 0;
  FLEXSPI_AHBRXBUF1CR0 = 0;
  FLEXSPI_AHBRXBUF2CR0 = 0;
  FLEXSPI_AHBRXBUF3CR0 = 0;
  FLEXSPI_AHBRXBUF4CR0 = 0;
  FLEXSPI_AHBRXBUF5CR0 = 0;
  FLEXSPI_AHBRXBUF6CR0 = 0;
  FLEXSPI_AHBRXBUF7CR0 = 0                                    // Use the same settings as the ROM bootloader.
                       | (16uL << AHBRXBUFCR_BUFSIZ_BIT)
                       | (7uL  << AHBRXBUFCR_MSTRID_BIT)
                       | (1uL  << AHBRXBUFCR_PREFETCHEN_BIT)
                       ;
  FLEXSPI_IPRXFCR      = ((FIFO_WATERMARK_RX / 8) - 1) << IPRXFCR_RXWMRK_BIT;
  FLEXSPI_IPTXFCR      = ((FIFO_WATERMARK_TX / 8) - 1) << IPTXFCR_TXWMRK_BIT;
  FLEXSPI_FLSHB1CR0    = (1uL << (MEM_SIZE_SHIFT - 10)) << FLSHCR0_FLSHSZ_BIT;
  FLEXSPI_FLSHB1CR1    = 0                                    // Use the same settings as the ROM bootloader.
                       | (3uL << FLSHCR1_TCSH_BIT)
                       | (3uL << FLSHCR1_TCSS_BIT)
                       ;
  FLEXSPI_FLSHB1CR2    = 0
                       | (LUT_INDEX_AHB_ACCESS << FLSHCR2_ARDSEQID_BIT)
                       ;
  FLEXSPI_FLSHA1CR0    = 0;
  FLEXSPI_FLSHA1CR1    = 0;
  FLEXSPI_FLSHA1CR2    = 0;
  FLEXSPI_FLSHA2CR0    = 0;
  FLEXSPI_FLSHA2CR1    = 0;
  FLEXSPI_FLSHA2CR2    = 0;
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
*       _HW_Unmap
*
*  Function description
*    Configures the hardware for direct access to serial NOR flash.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_Unmap(U8 Unit) {
  FS_USE_PARA(Unit);            // We support only the fist FlexSPI peripheral.
  //
  // This function is not required because the hardware can process
  // in parallel requests in command and memory mode.
  //
}

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
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_iMXRT685_NXP_MIMXRT685_EVK = {
  _HW_Init,
  _HW_Unmap,
  _HW_Map,
  _HW_Control,
  _HW_Read,
  _HW_Write,
  NULL,
  NULL,
  NULL,
  NULL
#if (FS_VERSION >= 51800u)
  , _HW_MapEx
  , _HW_ControlEx
  , _HW_ReadEx
  , _HW_WriteEx
  , NULL
#endif
};

/*************************** End of file ****************************/
