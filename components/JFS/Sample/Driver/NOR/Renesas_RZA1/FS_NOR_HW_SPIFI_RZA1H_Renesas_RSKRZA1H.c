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

File        : FS_NOR_HW_SPIFI_RZA1H_Renesas_RSKRZA1H.c
Purpose     : Low-level flash driver for Renesas RZA1H QSPI interface.
Literature  :
  [1] RZ/A1H Group, RZ/A1M Group User's Manual: Hardware
    (\\FILESERVER\Techinfo\Company\Renesas\MCU\RZA1\RZ_A1H_RZ_A1M_200_150811.pdf)
  [2] RSKRZA1H Schematics
    (\\FILESERVER\Techinfo\Company\Renesas\MCU\RZA1\EvalBoard\RSK\r20ut2586eg0100_rsk+rza1h_board_schematics.pdf)
  [3] S25FL512S Datasheet
    (\\FILESERVER\Techinfo\Company\Spansion\SPI_Flash\S25FL512S_00_Rev09.pdf)
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
#define QSPI_CLK_HZ           133000000uL   // Frequency of the clock supplied to QSPI unit.

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       QSPI unit
*/
#define QSPI_BASE_ADDR        0x3FEFA000uL
#define QSPI_CMNCR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x00))
#define QSPI_SSLDR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x04))
#define QSPI_SPBCR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x08))
#define QSPI_DRCR             (*(volatile U32*)(QSPI_BASE_ADDR + 0x0C))
#define QSPI_DRCMR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x10))
#define QSPI_DREAR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x14))
#define QSPI_DROPR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x18))
#define QSPI_DRENR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x1C))
#define QSPI_SMCR             (*(volatile U32*)(QSPI_BASE_ADDR + 0x20))
#define QSPI_SMCMR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x24))
#define QSPI_SMADR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x28))
#define QSPI_SMOPR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x2C))
#define QSPI_SMENR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x30))
#define QSPI_SMRDR0           (*(volatile U32*)(QSPI_BASE_ADDR + 0x38))
#define QSPI_SMRDR1           (*(volatile U32*)(QSPI_BASE_ADDR + 0x3C))
#define QSPI_SMWDR0           (*(volatile U32*)(QSPI_BASE_ADDR + 0x40))
#define QSPI_SMWDR1           (*(volatile U32*)(QSPI_BASE_ADDR + 0x44))
#define QSPI_CMNSR            (*(volatile U32*)(QSPI_BASE_ADDR + 0x48))
#define QSPI_DRDMCR           (*(volatile U32*)(QSPI_BASE_ADDR + 0x58))
#define QSPI_DRDRENR          (*(volatile U32*)(QSPI_BASE_ADDR + 0x5C))
#define QSPI_SMDMCR           (*(volatile U32*)(QSPI_BASE_ADDR + 0x60))
#define QSPI_SMDRENR          (*(volatile U32*)(QSPI_BASE_ADDR + 0x64))

/*********************************************************************
*
*       Port 9
*/
#define PORT_BASE             0xFCFE3000uL
#define PORT9_OFF             (9 * 4)
#define PORT9_PORT            (*((volatile U16*)(PORT_BASE + 0x0000 + PORT9_OFF)))
#define PORT9_PSR             (*((volatile U32*)(PORT_BASE + 0x0100 + PORT9_OFF)))
#define PORT9_PPR             (*((volatile U16*)(PORT_BASE + 0x0200 + PORT9_OFF)))
#define PORT9_PM              (*((volatile U16*)(PORT_BASE + 0x0300 + PORT9_OFF)))
#define PORT9_PMC             (*((volatile U16*)(PORT_BASE + 0x0400 + PORT9_OFF)))
#define PORT9_PFC             (*((volatile U16*)(PORT_BASE + 0x0500 + PORT9_OFF)))
#define PORT9_PFCE            (*((volatile U16*)(PORT_BASE + 0x0600 + PORT9_OFF)))
#define PORT9_PNOT            (*((volatile U16*)(PORT_BASE + 0x0700 + PORT9_OFF)))
#define PORT9_PMSR            (*((volatile U32*)(PORT_BASE + 0x0800 + PORT9_OFF)))
#define PORT9_PMCSR           (*((volatile U32*)(PORT_BASE + 0x0900 + PORT9_OFF)))
#define PORT9_PFCAE           (*((volatile U16*)(PORT_BASE + 0x0A00 + PORT9_OFF)))
#define PORT9_PIBC            (*((volatile U32*)(PORT_BASE + 0x4000 + PORT9_OFF)))
#define PORT9_PIBD            (*((volatile U32*)(PORT_BASE + 0x4100 + PORT9_OFF)))
#define PORT9_PIPC            (*((volatile U16*)(PORT_BASE + 0x4200 + PORT9_OFF)))

/*********************************************************************
*
*       Misc. SFRs
*/
#define STBCR9                (*((volatile U8*)0xFCFE0438))

/*********************************************************************
*
*       Common Control Register
*/
#define CMNCR_CPOL_BIT        3
#define CMNCR_CPHAR_BIT       5
#define CMNCR_CPHAT_BIT       6
#define CMNCR_IO2FV_BIT       12
#define CMNCR_IO3FV_BIT       14
#define CMNCR_IOFV_HIGH       1uL
#define CMNCR_MOIIO0_BIT      16
#define CMNCR_MOIIO1_BIT      18
#define CMNCR_MOIIO2_BIT      20
#define CMNCR_MOIIO3_BIT      22
#define CMNCR_MOIIO_KEEP      2uL
#define CMNCR_MOIIO_HIGH      1uL
#define CMNCR_SFDE_BIT        24
#define CNMCR_MD_BIT          31

/*********************************************************************
*
*       SSL Delay Register
*/
#define SSLDR_SCKDL_BIT       0
#define SSLDR_SLNDL_BIT       8
#define SSLDR_SPNDL_BIT       16

/*********************************************************************
*
*       Data Read Control Register
*/
#define SPBCR_SPBR_BIT        8

/*********************************************************************
*
*       Data Read Control Register
*/
#define DRCR_SSLE_BIT         0
#define DRCR_RBE_BIT          8
#define DRCR_RCF_BIT          9
#define DRCR_RBURST_BIT       16
#define DRCR_SSLN_BIT         24

/*********************************************************************
*
*       Data Read Command Setting Register
*/
#define DRCMR_CMD_BIT         16

/*********************************************************************
*
*       Data Read Enable Setting Register
*/
#define DRENR_SPIDE_BIT       0
#define DRENR_OPDE_BIT        4
#define DRENR_ADE_BIT         8
#define DRENR_CDE_BIT         14
#define DRENR_SPIDB_BIT       16
#define DRENR_OPDB_BIT        20
#define DRENR_ADB_BIT         24
#define DRENR_CDB_BIT         30

/*********************************************************************
*
*       SPI Mode Control Register
*/
#define SMCR_SPIE_BIT         0
#define SMCR_SPIWE_BIT        1
#define SMCR_SPIRE_BIT        2
#define SMCR_SSLLP_BIT        8

/*********************************************************************
*
*       SPI Mode Command Setting Register
*/
#define SMCMR_CMD_BIT         16

/*********************************************************************
*
*       SPI Mode Enable Setting Register
*/
#define SMENR_SPIDE_BIT       0
#define SMENR_SPIDE_8BIT      0x8uL
#define SMENR_OPDE_BIT        4
#define SMENR_OPDE_MASK       0xFuL
#define SMENR_ADE_BIT         8
#define SMENR_ADE_MASK        0xFuL
#define SMENR_CDE_BIT         14
#define SMENR_SPIDB_BIT       16
#define SMENR_OPDB_BIT        20
#define SMENR_ADB_BIT         24
#define SMENR_CDB_BIT         30

/*********************************************************************
*
*       Common Status Register
*/
#define CMNSR_SSLF_BIT        1
#define CMNSR_TEND_BIT        0

/*********************************************************************
*
*       NOR pins
*/
#define NOR_CLK_BIT           2   // Port 9
#define NOR_CS_BIT            3   // Port 9
#define NOR_IO0_BIT           4   // Port 9
#define NOR_IO1_BIT           5   // Port 9
#define NOR_IO2_BIT           6   // Port 9
#define NOR_IO3_BIT           7   // Port 9

/*********************************************************************
*
*       Misc. defines
*/
#define PFC_ALT2              1
#define STBCR9_MSTP92_BIT     2
#define STBCR9_MSTP93_BIT     3

/*********************************************************************
*
*      Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _GetBitSize
*/
static U32 _GetBitSize(int BusWidth) {
  switch (BusWidth) {
  case 1:
    //through
  default:
    return 0;
  case 2:
    return 1;
  case 4:
    return 2;
  }
}

/*********************************************************************
*
*       _GetOutputTypeOpt
*/
static U32 _GetOutputTypeOpt(unsigned NumBytes) {
  switch (NumBytes) {
  case 0:
    //through
  default:
    return 0;
  case 1:
    return 0x8;
  case 2:
    return 0xC;
  case 3:
    return 0xE;
  case 4:
    return 0xF;
  }
}

/*********************************************************************
*
*       _GetOutputTypeAddr
*/
static U32 _GetOutputTypeAddr(unsigned NumBytes) {
  switch (NumBytes) {
  case 0:
    //through
  default:
    return 0;
  case 1:
    return 0x4;
  case 2:
    return 0x6;
  case 3:
    return 0x7;
  case 4:
    return 0xF;
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
  int          Freq_Hz;
  U8           v;
  volatile U32 Dummy;

  FS_USE_PARA(Unit);    // This device has only one HW unit.
  //
  // Start the QSPI units if required.
  //
  v = STBCR9;
  if (v & (1 << STBCR9_MSTP92_BIT)) {
    v &= ~(1 << STBCR9_MSTP92_BIT);
  }
  if (v & (1 << STBCR9_MSTP93_BIT)) {
    v &= ~(1 << STBCR9_MSTP93_BIT);
  }
  STBCR9 = v;
  //
  // Configure the operating mode.
  //
  QSPI_CMNCR = 0
             | (1uL              << CMNCR_SFDE_BIT)     // Perform data swapping in 8-bit units
             | (CMNCR_MOIIO_HIGH << CMNCR_MOIIO3_BIT)   // I/O3 state no changed after transfer finished
             | (CMNCR_MOIIO_HIGH << CMNCR_MOIIO2_BIT)   // I/O2 state no changed after transfer finished
             | (CMNCR_MOIIO_HIGH << CMNCR_MOIIO1_BIT)   // I/O1 state no changed after transfer finished
             | (CMNCR_MOIIO_HIGH << CMNCR_MOIIO0_BIT)   // I/O0 state no changed after transfer finished
             | (CMNCR_IOFV_HIGH  << CMNCR_IO2FV_BIT)    // I/O2 output == HIGH for 1-2 bit transfers
             | (CMNCR_IOFV_HIGH  << CMNCR_IO3FV_BIT)    // I/O3 output == HIGH for 1-2 bit transfers
             | (1uL              << CMNCR_CPOL_BIT)
             | (1uL              << CMNCR_CPHAR_BIT)
             | (1uL              << CMNCR_CPHAT_BIT)
             ;
  //
  // Configure the timing between the chip select and clock signals.
  //
  QSPI_SSLDR = 0
             | (7uL << SSLDR_SPNDL_BIT)       // Next Access Delay
             | (7uL << SSLDR_SLNDL_BIT)       // SPBSSL Negation Delay
             | (7uL << SSLDR_SCKDL_BIT)       // Clock Delay
             ;
  //
  // Configure the clock speed. The Renesas RZ does not really have a modifiable PLL. Just the CPU clock can be adjusted.
  // The max. clock the QSPI can run at is 66 MHz (bus clock / 2). B clock is usually 128 or 133 MHz, we assume 133 MHz
  // and select a safe speed of 133 / 4 == 33.33 MHz which should work for all cases
  // Formula: Baud = BCLK / (2 * n / 2^N)(n == x << 8)
  //
  QSPI_SPBCR = 2 << SPBCR_SPBR_BIT;
  //
  // Initialize DRCR register to make sure that burst read accesses as well as read cache is enabled in memory mapped mode
  //
  QSPI_DRCR = 0
            | (0xFuL << DRCR_RBURST_BIT)    // Set burst length to 16 units, 64-bits each
            | (1uL   << DRCR_RBE_BIT)       // Enable read cache & burst mode
            | (1uL   << DRCR_SSLE_BIT)      // Only negate nCS in case non-continuous read accesses are made
            ;
  QSPI_DRCR |= (1uL << DRCR_RCF_BIT);       // Make sure that read cache of QSPI unit is invalidated, since flash content has changed
  Dummy = QSPI_DRCR;                        // Dummy read from DRCR is necessary after read cache invalidate
  FS_USE_PARA(Dummy);
  //
  // Configure as input to avoid glitches during configuration.
  //
  PORT9_PM    |= 0
              | (1uL << NOR_CLK_BIT)
              | (1uL << NOR_CS_BIT)
              | (1uL << NOR_IO0_BIT)
              | (1uL << NOR_IO1_BIT)
              | (1uL << NOR_IO2_BIT)
              | (1uL << NOR_IO3_BIT)
              ;
  //
  // Configure the pins as GPIOs.
  //
  PORT9_PMC   &= ~((1uL << NOR_CLK_BIT) |
                   (1uL << NOR_CS_BIT)  |
                   (1uL << NOR_IO0_BIT) |
                   (1uL << NOR_IO1_BIT) |
                   (1uL << NOR_IO2_BIT) |
                   (1uL << NOR_IO3_BIT));
  //
  // Use direction settings from PMC
  //
  PORT9_PIPC  &= ~((1uL << NOR_CLK_BIT) |
                   (1uL << NOR_CS_BIT)  |
                   (1uL << NOR_IO0_BIT) |
                   (1uL << NOR_IO1_BIT) |
                   (1uL << NOR_IO2_BIT) |
                   (1uL << NOR_IO3_BIT));
  //
  // Clear ALTFunc[2:2]
  //
  PORT9_PFCAE &= ~((1uL << NOR_CLK_BIT) |
                   (1uL << NOR_CS_BIT)  |
                   (1uL << NOR_IO0_BIT) |
                   (1uL << NOR_IO1_BIT) |
                   (1uL << NOR_IO2_BIT) |
                   (1uL << NOR_IO3_BIT));
  PORT9_PFCE  &= ~((1uL << NOR_CLK_BIT) |
                   (1uL << NOR_CS_BIT)  |
                   (1uL << NOR_IO0_BIT) |
                   (1uL << NOR_IO1_BIT) |
                   (1uL << NOR_IO2_BIT) |
                   (1uL << NOR_IO3_BIT));
  PORT9_PFC   &= ~((1uL << NOR_CLK_BIT) |
                   (1uL << NOR_CS_BIT)  |
                   (1uL << NOR_IO0_BIT) |
                   (1uL << NOR_IO1_BIT) |
                   (1uL << NOR_IO2_BIT) |
                   (1uL << NOR_IO3_BIT));
  //
  // Set ALTFunc[2:2]
  //
  PORT9_PFC   |= 0
              | (1 << NOR_CLK_BIT)
              | (1 << NOR_CS_BIT)
              | (1 << NOR_IO0_BIT)
              | (1 << NOR_IO1_BIT)
              | (1 << NOR_IO2_BIT)
              | (1 << NOR_IO3_BIT)
              ;
  //
  // ALTx mode (non-GPIO) for pin.
  //
  PORT9_PMC   |= 0
              | (1uL << NOR_CLK_BIT)
              | (1uL << NOR_CS_BIT)
              | (1uL << NOR_IO0_BIT)
              | (1uL << NOR_IO1_BIT)
              | (1uL << NOR_IO2_BIT)
              | (1uL << NOR_IO3_BIT)
              ;
  //
  // Port I/O mode is controlled by peripheral controlling the pin
  //
  PORT9_PIPC  |= 0
              | (1uL << NOR_CLK_BIT)
              | (1uL << NOR_CS_BIT)
              | (1uL << NOR_IO0_BIT)
              | (1uL << NOR_IO1_BIT)
              | (1uL << NOR_IO2_BIT)
              | (1uL << NOR_IO3_BIT)
              ;
  //
  // Return the actual clock frequency.
  //
  Freq_Hz = QSPI_CLK_HZ >> 2;
  return Freq_Hz;
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
  FS_USE_PARA(Unit);
  QSPI_DRCR |= 1uL << DRCR_SSLN_BIT;                      // Force negate of nCS
  while (QSPI_CMNSR & (1uL << CMNSR_SSLF_BIT)) {          // Wait until nCS is negated
    ;
  }
  while ((QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) == 0) {   // Wait until SPI is ready again
    ;
  }
  QSPI_CMNCR |= 1uL << CNMCR_MD_BIT;                      // Set QSPI unit to SPI mode (non-memory mapped mode)
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
  U32          BitSizeCmd;
  U32          BitSizeAddr;
  U32          BitSizeData;
  U32          OutputTypeAddr;
  U32          OutputTypeOpt;
  volatile U32 Dummy;

  FS_USE_PARA(Unit);
  BitSizeCmd     = _GetBitSize(FS_BUSWIDTH_GET_CMD(BusWidth));
  BitSizeAddr    = _GetBitSize(FS_BUSWIDTH_GET_ADDR(BusWidth));
  BitSizeData    = _GetBitSize(FS_BUSWIDTH_GET_DATA(BusWidth));
  OutputTypeAddr = _GetOutputTypeAddr(NumBytesAddr);
  OutputTypeOpt  = _GetOutputTypeOpt(NumBytesDummy);
  //
  // Set QSPI unit to memory-mapped mode
  //
  QSPI_CMNCR &= ~(1uL << CNMCR_MD_BIT);
  //
  // Configure the read access mode.
  //
  QSPI_DRCMR = ReadCmd << DRCMR_CMD_BIT;
  QSPI_DRENR = 0
             | (BitSizeCmd       << DRENR_CDB_BIT)
             | (BitSizeAddr      << DRENR_OPDB_BIT)
             | (BitSizeAddr      << DRENR_ADB_BIT)
             | (BitSizeData      << DRENR_SPIDB_BIT)
             | (1uL              << DRENR_CDE_BIT)
             | (OutputTypeAddr   << DRENR_ADE_BIT)
             | (OutputTypeOpt    << DRENR_OPDE_BIT)
             ;
  //
  // RZ specific: In case 4-byte addresses are used, make sure that QSPI controller
  // can control addr. bits 0-25 dynamically so we can access the complete 64 MB QSPI area of the RZ
  // In all other cases it is enough if it can control addr. bits 0-24 dynamically
  //
  if (NumBytesAddr == 4) {
    QSPI_DREAR = 1;
  } else {
    QSPI_DREAR = 0;
  }
  //
  // Make sure that read cache of QSPI unit is invalidated, since flash content has changed
  // A dummy read from DRCR is necessary after read cache invalidate.
  //
  QSPI_DRCR |= (1uL << DRCR_RCF_BIT);
  Dummy = QSPI_DRCR;
  FS_USE_PARA(Dummy);
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
  U32 BitSizeCmd;

  FS_USE_PARA(Unit);
  BitSizeCmd = _GetBitSize(BusWidth);
  QSPI_SMADR = 0;
  QSPI_SMOPR = 0;
  QSPI_SMCMR = (U32)Cmd << SMCMR_CMD_BIT;
  QSPI_SMENR = 0
             | (BitSizeCmd << SMENR_CDB_BIT)
             | (1uL        << SMENR_CDE_BIT)
             ;
  //
  // Send the command and the address.
  //
  QSPI_SMCR = (1uL << SMCR_SPIE_BIT);
  //
  // Wait until the command is sent.
  //
  while (1) {
    if (QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) {
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
  U32      BitSizeCmd;
  U32      BitSizeAddr;
  U32      BitSizeData;
  U32      OutputTypeAddr;
  U32      OutputTypeOpt;
  U32      RegSMADR;
  U32      RegSMOPR;
  unsigned NumBytesOpt;
  U32      KeepCSAsserted;

  FS_USE_PARA(Unit);
  NumBytesOpt    = NumBytesPara - NumBytesAddr;
  BitSizeCmd     = _GetBitSize(FS_BUSWIDTH_GET_CMD(BusWidth));
  BitSizeAddr    = _GetBitSize(FS_BUSWIDTH_GET_ADDR(BusWidth));
  BitSizeData    = _GetBitSize(FS_BUSWIDTH_GET_DATA(BusWidth));
  OutputTypeAddr = _GetOutputTypeAddr(NumBytesAddr);
  OutputTypeOpt  = _GetOutputTypeOpt(NumBytesOpt);
  //
  // Prepare the address bytes.
  //
  RegSMADR = 0;
  if (NumBytesAddr == 1) {
    RegSMADR = (U32)*pPara << 16;
  }
  if (NumBytesAddr == 2) {
    RegSMADR  = (U32)*pPara++ << 16;
    RegSMADR |= (U32)*pPara   << 8;
  }
  if ((NumBytesAddr == 4) || ((NumBytesAddr == 3))) {
    do {
      RegSMADR <<= 8;
      RegSMADR  |= (U32)(*pPara++);
    } while (--NumBytesAddr);
  }
  //
  // Prepare the mode and dummy bytes.
  //
  RegSMOPR = 0;
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 24;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 16;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 8;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++);
  }

  QSPI_SMADR = RegSMADR;
  QSPI_SMOPR = RegSMOPR;
  QSPI_SMCMR = (U32)Cmd << SMCMR_CMD_BIT;
  QSPI_SMENR = 0
             | (BitSizeCmd       << SMENR_CDB_BIT)
             | (BitSizeAddr      << SMENR_OPDB_BIT)
             | (BitSizeAddr      << SMENR_ADB_BIT)
             | (BitSizeData      << SMENR_SPIDB_BIT)
             | (1uL              << SMENR_CDE_BIT)
             | (OutputTypeAddr   << SMENR_ADE_BIT)
             | (OutputTypeOpt    << SMENR_OPDE_BIT)
             ;
  //
  // Send the command and the address.
  //
  QSPI_SMCR = 0
            | (1uL << SMCR_SSLLP_BIT)
            | (1uL << SMCR_SPIE_BIT)
            ;
  //
  // Wait until the command is sent.
  //
  while (1) {
    if (QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) {
      break;
    }
  }
  //
  // Disable the transfer of command, address and options and enable the transfer of data.
  //
  QSPI_SMENR &= ~((1uL             << SMENR_CDE_BIT) |
                  (SMENR_ADE_MASK  << SMENR_ADE_BIT) |
                  (SMENR_OPDE_MASK << SMENR_OPDE_BIT));
  QSPI_SMENR = 0
             | (SMENR_SPIDE_8BIT << SMENR_SPIDE_BIT)
             ;
  //
  // Transfer data.
  //
  do {
    KeepCSAsserted = 1;
    if (NumBytesData == 1) {
      KeepCSAsserted = 0;       // Deassert CS after the last byte is transferred.
    }
    //
    // Start a new data transfer.
    //
    QSPI_SMCR = 0
              | (KeepCSAsserted << SMCR_SSLLP_BIT)
              | (1uL            << SMCR_SPIRE_BIT)
              | (1uL            << SMCR_SPIE_BIT)
              ;
    //
    // Wait until the data transfer is ready.
    //
    while (1) {
      if (QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) {
        break;
      }
    }
    //
    // Move the read data from register to user buffer.
    //
    *pData++ = (U8)QSPI_SMRDR0;
  } while (--NumBytesData);
  //
  // Wait until nCS is de-asserted.
  //
  while (QSPI_CMNSR & (1uL << CMNSR_SSLF_BIT)) {
    ;
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
  U32      BitSizeCmd;
  U32      BitSizeAddr;
  U32      BitSizeData;
  U32      OutputTypeAddr;
  U32      OutputTypeOpt;
  U32      RegSMADR;
  U32      RegSMOPR;
  unsigned NumBytesOpt;
  U32      KeepCSAsserted;

  FS_USE_PARA(Unit);
  NumBytesOpt    = NumBytesPara - NumBytesAddr;
  BitSizeCmd     = _GetBitSize(FS_BUSWIDTH_GET_CMD(BusWidth));
  BitSizeAddr    = _GetBitSize(FS_BUSWIDTH_GET_ADDR(BusWidth));
  BitSizeData    = _GetBitSize(FS_BUSWIDTH_GET_DATA(BusWidth));
  OutputTypeAddr = _GetOutputTypeAddr(NumBytesAddr);
  OutputTypeOpt  = _GetOutputTypeOpt(NumBytesOpt);
  //
  // Prepare the address bytes.
  //
  RegSMADR = 0;
  if (NumBytesAddr == 1) {
    RegSMADR = (U32)*pPara << 16;
  }
  if (NumBytesAddr == 2) {
    RegSMADR  = (U32)*pPara++ << 16;
    RegSMADR |= (U32)*pPara   << 8;
  }
  if ((NumBytesAddr == 4) || ((NumBytesAddr == 3))) {
    do {
      RegSMADR <<= 8;
      RegSMADR  |= (U32)(*pPara++);
    } while (--NumBytesAddr);
  }
  //
  // Prepare the mode and dummy bytes.
  //
  RegSMOPR = 0;
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 24;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 16;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++) << 8;
    --NumBytesOpt;
  }
  if (NumBytesOpt) {
    RegSMOPR |= (U32)(*pPara++);
  }

  QSPI_SMADR = RegSMADR;
  QSPI_SMOPR = RegSMOPR;
  QSPI_SMCMR = (U32)Cmd << SMCMR_CMD_BIT;
  QSPI_SMENR = 0
             | (BitSizeCmd       << SMENR_CDB_BIT)
             | (BitSizeAddr      << SMENR_OPDB_BIT)
             | (BitSizeAddr      << SMENR_ADB_BIT)
             | (BitSizeData      << SMENR_SPIDB_BIT)
             | (1uL              << SMENR_CDE_BIT)
             | (OutputTypeAddr   << SMENR_ADE_BIT)
             | (OutputTypeOpt    << SMENR_OPDE_BIT)
             ;
  //
  // Send the command and the address.
  //
  QSPI_SMCR = 0
            | (1uL << SMCR_SSLLP_BIT)
            | (1uL << SMCR_SPIE_BIT)
            ;
  //
  // Wait until the command is sent.
  //
  while (1) {
    if (QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) {
      break;
    }
  }
  //
  // Disable the transfer of command, address and options and enable the transfer of data.
  //
  QSPI_SMENR &= ~((1uL             << SMENR_CDE_BIT) |
                  (SMENR_ADE_MASK  << SMENR_ADE_BIT) |
                  (SMENR_OPDE_MASK << SMENR_OPDE_BIT));
  QSPI_SMENR |= 0
             | (SMENR_SPIDE_8BIT << SMENR_SPIDE_BIT)
             ;
  //
  // Transfer data.
  // TBD: Transfer more than 1 byte at a time to increase performance.
  //
  do {
    //
    // Move the read data from register to user buffer.
    //
    QSPI_SMWDR0 = ((U32)*pData++) << 24;
    //
    // Start a new data transfer.
    //
    KeepCSAsserted = 1;
    if (NumBytesData == 1) {
      KeepCSAsserted = 0;       // De-assert CS after the last byte is transferred.
    }
    QSPI_SMCR = 0
              | (KeepCSAsserted << SMCR_SSLLP_BIT)
              | (1uL            << SMCR_SPIWE_BIT)
              | (1uL            << SMCR_SPIE_BIT)
              ;
    //
    // Wait until the data transfer is ready.
    //
    while (1) {
      if (QSPI_CMNSR & (1uL << CMNSR_TEND_BIT)) {
        break;
      }
    }
  } while (--NumBytesData);
  //
  // Wait until nCS is de-asserted.
  //
  while (QSPI_CMNSR & (1uL << CMNSR_SSLF_BIT)) {
    ;
  }
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_RZA1H_Renesas_RSKRZA1H = {
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
