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

File    : FS_NOR_HW_SPI_RX65N_Renesas_RSKRX65N.c
Purpose : NOR SPI hardware layer for RSKRX65N evaluation board.
Literature:
  [1] RX65N Group, RX651 Group User's Manual: Hardware
    (\\FILESERVER\Techinfo\Company\Renesas\MCU\RX\RX65N\r01uh0590ej0210_RX65N_RX651_UserManual.pdf)
  [2] Renesas Starter Kit+ for RX65N & RX651 CPU Board Schematics
    (\\fileserver\Techinfo\Company\Renesas\MCU\RX\EvalBoard\RSK_RX65N\RSK+RX65N_Schema_Rev.0.05.pdf)

These are the port pin assignments and jumper settings that are used
on the RSKRX65N evaluation board:

Signal name   Port  Jumper setting    Peripheral
------------------------------------------------
SSLA0-B       PA4   J14, 2-3          GPIO
RSPCKA-B      PA5   J12, 2-3          RSPI
MOSIA-B       PA6   J13, 2-3          RSPI
MISOA-B       PA7   J11, 2-3          RSPI
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
#ifndef   FS_NOR_HW_SPI_PER_CLOCK_KHZ
  #define FS_NOR_HW_SPI_PER_CLOCK_KHZ       120000uL  // Clock of SPI peripheral in kHz
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLOCK_KHZ
  #define FS_NOR_HW_SPI_NOR_CLOCK_KHZ       60000u    // SPI communication speed.
#endif

#ifndef   FS_NOR_HW_SPI_USE_OS
  #define FS_NOR_HW_SPI_USE_OS              0         // Operating mode: 0: polling, 1: interrupt.
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_SPI_USE_OS
  #include "RTOS.h"
  #include "FS_OS.h"
#endif // FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       defines, non-configurable
*
**********************************************************************
*/
#if defined __RX
  #define EVENACCESS  __evenaccess
#else
  #define EVENACCESS
#endif

/*********************************************************************
*
*       Serial Peripheral Interface (RSPI)
*/
#define RSPI_BASE_ADDR    0x000D0100
#define RSPI_SPCR         (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x00))   // Control register
#define RSPI_SPSR         (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x03))   // Status register
#define RSPI_SPDR         (*(volatile EVENACCESS U32 *)(RSPI_BASE_ADDR + 0x04))   // Data register (32-bit)
#define RSPI_SPSCR        (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x08))   // Sequence control register
#define RSPI_SPBR         (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x0A))   // Bit rate register
#define RSPI_SPDCR        (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x0B))   // Data control register
#define RSPI_SPCR2        (*(volatile EVENACCESS U8 *) (RSPI_BASE_ADDR + 0x0F))   // Control register 2
#define RSPI_SPCMD0       (*(volatile EVENACCESS U16 *)(RSPI_BASE_ADDR + 0x10))   // Command register 0

/*********************************************************************
*
*       I/O Ports
*/
#define PA_BASE_ADDR      0x0008C00A
#define PA_PDR            (*(volatile EVENACCESS U8 *)(PA_BASE_ADDR + 0x00))      // Port direction register
#define PA_PODR           (*(volatile EVENACCESS U8 *)(PA_BASE_ADDR + 0x20))      // Port output data register
#define PA_PMR            (*(volatile EVENACCESS U8 *)(PA_BASE_ADDR + 0x60))      // Port mode register

/*********************************************************************
*
*       Multi-function pin controller (MPC)
*/
#define MPC_PWPR          (*(volatile EVENACCESS U8 *)0x0008C11F)                 // Write-protect register
#define MPC_PFBCR0        (*(volatile EVENACCESS U8 *)0x0008C106)                 // External Bus Control Register 0
#define MPC_PA4PFS        (*(volatile EVENACCESS U8 *)0x0008C194)                 // Port A pin 4 function select register
#define MPC_PA5PFS        (*(volatile EVENACCESS U8 *)0x0008C195)                 // Port A pin 5 function select register
#define MPC_PA6PFS        (*(volatile EVENACCESS U8 *)0x0008C196)                 // Port A pin 6 function select register
#define MPC_PA7PFS        (*(volatile EVENACCESS U8 *)0x0008C197)                 // Port A pin 7 function select register

/*********************************************************************
*
*       Misc. registers
*/
#define MSTPCRB           (*(volatile EVENACCESS U32 *)0x00080014)                // Module stop control register B
#define PRCR              (*(volatile EVENACCESS U16 *)0x000803FE)                // Write protect register
#define IER04             (*(volatile EVENACCESS U8  *)0x00087204)                // Interrupt enable register 5
#define IR038_SPRI1       (*(volatile EVENACCESS U8  *)0x00087026)                // Interrupt request register 38 (receive)
#define IR039_SPRI1       (*(volatile EVENACCESS U8  *)0x00087027)                // Interrupt request register 39 (transmit)
#define IPR38_SPRI1       (*(volatile EVENACCESS U8  *)0x00087326)                // Interrupt priority register 38 (receive)
#define IPR39_SPTI1       (*(volatile EVENACCESS U8  *)0x00087327)                // Interrupt priority register 39 (transmit)

/*********************************************************************
*
*       RSPI port function control bits
*/
#define PFGSPI_RSPCKE     1
#define PFGSPI_MOSIE      2
#define PFGSPI_MISOE      3

/*********************************************************************
*
*       RSPI control register bits
*/
#define SPCR_SPMS         0
#define SPCR_MODFEN       2
#define SPCR_MSTR         3
#define SPCR_SPTIE        5
#define SPCR_SPE          6
#define SPCR_SPRIE        7

/*********************************************************************
*
*       RSPI status register bits
*/
#define SPSR_OVRF         0
#define SPSR_ILDNF        1
#define SPSR_MODF         2
#define SPSR_PERF         3

/*********************************************************************
*
*       RSPI data control register bits
*/
#define SPDCR_SPLW        5

/*********************************************************************
*
*       RSPI command register bits
*/
#define SPCMD_CHPA        0
#define SPCMD_CPOL        1
#define SPCMD_SPB         8

/*********************************************************************
*
*       Interrupt vectors
*/
#define VECT_SPRI         38
#define VECT_SPTI         39

/*********************************************************************
*
*       Misc. bit defines
*/
#define NOR_CS_PIN        4   // Chip select         (Port A)
#define NOR_CLK_PIN       5   // Clock               (Port A)
#define NOR_MOSI_PIN      6   // Master out slave in (Port A)
#define NOR_MISO_PIN      7   // Master in slave out (Port A)
#define MSTPCRB_RSPI      17  // Start/Stop RSPI
#define PRCR_PRC1         1   // Write protect
#define PRCR_PRKEY        8   // Write protect key
#define PRCR_MAGIC        0xA5
#define PWPR_B0WI         7
#define PWPR_PFSWE        6
#define WAIT_TIMEOUT_MS   1000
#define IER04_RI          6
#define IER04_TI          7
#define ISR_PRIO          2   // 0 is the lowest priority
#define PFBCR0_ADRLE      0

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
#if FS_NOR_HW_SPI_USE_OS
  static volatile U8 _TxBufferEmpty;
  static volatile U8 _RxBufferReady;
#endif // FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

#if FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       _ISR_Rx()
*
*  Function description
*    Interrupt handler for RSPI receiver buffer ready event
*/
#ifdef __ICCRX__
  #pragma vector = (VECT_SPRI)
  static __interrupt void          _ISR_Rx(void) {
#endif
#ifdef __RX
  #pragma interrupt (_ISR_Rx(vect=VECT_SPRI))
  static void                      _ISR_Rx(void) {
#endif
#ifdef __GNUC__
  void __attribute__ ((interrupt)) _ISR_Rx(void) {
#endif
  OS_EnterNestableInterrupt();
  _RxBufferReady = 1;
  FS_OS_Signal();
  OS_LeaveNestableInterrupt();
}

/*********************************************************************
*
*       _ISR_Tx()
*
*  Function description
*    Interrupt handler for RSPI transmitter buffer empty event
*/
#ifdef __ICCRX__
  #pragma vector = (VECT_SPTI)
  static __interrupt void          _ISR_Tx(void) {
#endif
#ifdef __RX
  #pragma interrupt (_ISR_Tx(vect=VECT_SPTI))
  static void                      _ISR_Tx(void) {
#endif
#ifdef __GNUC__
  void __attribute__ ((interrupt)) _ISR_Tx(void) {
#endif
  OS_EnterNestableInterrupt();
  _TxBufferEmpty = 1;
  FS_OS_Signal();
  OS_LeaveNestableInterrupt();
}

#endif

/*********************************************************************
*
*       _LoadU32BE
*
*  Function description
*    Reads a 32 bit value stored in big endian format from a byte array.
*/
static U32 _LoadU32BE(const U8 *pBuffer) {
  U32 r;
  r = *pBuffer++;
  r = (r << 8) | *pBuffer++;
  r = (r << 8) | *pBuffer++;
  r = (r << 8) | *pBuffer;
  return r;
}

/*********************************************************************
*
*       _ClearErrors
*
*   Function description
*     Performs error recovery. Only the receiver overrun error is handled.
*/
static void _ClearErrors(void) {
  volatile U32 Data;

  //
  // Clear an overrun error if required.
  //
  if (RSPI_SPSR & (1 << SPSR_OVRF)) {
    RSPI_SPSR &= ~(1 << SPSR_OVRF);
    Data        = RSPI_SPDR;
    RSPI_SPCR &= ~(1 << SPCR_SPE);
    RSPI_SPCR |=  (1 << SPCR_SPE);
  }
}

/*********************************************************************
*
*       _WaitForTxBufferEmpty
*
*   Function description
*     Waits for RSPI to move the data from the transmit buffer to shift register.
*/
static void _WaitForTxBufferEmpty(void) {
  //
  // Wait for the data to be moved to transmit shift register.
  //
#if FS_NOR_HW_SPI_USE_OS
  while (_TxBufferEmpty == 0) {
    FS_OS_Wait(WAIT_TIMEOUT_MS);
  }
#else
  while (IR039_SPRI1 == 0) {
    ;
  }
  IR039_SPRI1 = 0;
#endif // FS_NOR_HW_SPI_USE_OS
}

/*********************************************************************
*
*       _WaitForRxBufferReady
*
*   Function description
*     Waits for RSPI to fill the receive buffer with data.
*/
static void _WaitForRxBufferReady(void) {
  //
  // Wait for the data to be received and moved to receive data register.
  //
#if FS_NOR_HW_SPI_USE_OS
  while (_RxBufferReady == 0) {
    FS_OS_Wait(WAIT_TIMEOUT_MS);
  }
#else
  while (IR038_SPRI1 == 0) {
    ;
  }
  IR038_SPRI1 = 0;
#endif // FS_NOR_HW_SPI_USE_OS
}

/*********************************************************************
*
*       _EnableInt
*
*   Function description
*     Enables the RSPI interrupts.
*/
static void _EnableInt(void) {
#if FS_NOR_HW_SPI_USE_OS
  IPR38_SPRI1 = ISR_PRIO;
  IPR39_SPTI1 = ISR_PRIO;
  IER04 |= (1 << IER04_RI)
        |  (1 << IER04_TI)
        ;
#endif // FS_NOR_HW_SPI_USE_OS
}

/*********************************************************************
*
*       _DisableInt
*
*   Function description
*     Disables the RSPI interrupts.
*/
static void _DisableInt(void) {
#if FS_NOR_HW_SPI_USE_OS
  IPR38_SPRI1 = 0;
  IPR39_SPTI1 = 0;
  IER04 &= ~((1 << IER04_RI) |
             (1 << IER04_TI));
#endif // FS_NOR_HW_SPI_USE_OS
}

/*********************************************************************
*
*       _Read8Bits
*
*   Function description
*     Receives 8 bits from SPI flash.
*/
static void _Read8Bits(U8 * pData) {
  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif // FS_NOR_HW_SPI_USE_OS
  //
  // Transfer 8 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x7u << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI_SPDCR  &= ~0x3u;
  //
  // Start the data transfer.
  //
  RSPI_SPDR    = 0xFFu;
  _WaitForRxBufferReady();
  //
  // Store data to receive buffer.
  //
  *pData = (U8)RSPI_SPDR;
}

/*********************************************************************
*
*       _Read32Bits
*
*   Function description
*     Receives 32 bits from SPI flash.
*/
static void _Read32Bits(U32 * pData) {
  U32 Data32;

  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif // FS_NOR_HW_SPI_USE_OS
  //
  // Transfer 32 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x3u << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI_SPDCR  &= ~0x3u;
  //
  // Start the data transfer.
  //
  RSPI_SPDR = 0xFFFFFFFFuL;
  _WaitForRxBufferReady();
  Data32   = RSPI_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
}

/*********************************************************************
*
*       _Read128Bits
*
*   Function description
*     Receives 128 bits from SPI flash.
*/
static void _Read128Bits(U32 * pData) {
  U32 Data32;

  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif // FS_NOR_HW_SPI_USE_OS
  //
  // Transfer 32 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x3u << SPCMD_SPB;
  //
  // Perform 4 transfers (128 bits in total).
  //
  RSPI_SPDCR  |= 0x3u;
  //
  // Data transfer starts when the last word is stored to buffer.
  //
  RSPI_SPDR = 0xFFFFFFFFuL;
  RSPI_SPDR = 0xFFFFFFFFuL;
  RSPI_SPDR = 0xFFFFFFFFuL;
  RSPI_SPDR = 0xFFFFFFFFuL;
  _WaitForRxBufferReady();
  Data32   = RSPI_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
}

/*********************************************************************
*
*       _Write8Bits
*
*   Function description
*     Sends 8 bits over SPI to SPI flash.
*/
static void _Write8Bits(U8 Data) {
  volatile U32 Dummy32;

  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif
  //
  // Transfer 8 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x7u << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI_SPDCR  &= ~0x3u;
  //
  // Start the data transfer.
  //
  RSPI_SPDR    = (U32)Data;
  _WaitForRxBufferReady();
  //
  // Read data from buffer to prevent a receive overrun error.
  //
  Dummy32 = RSPI_SPDR;
}

/*********************************************************************
*
*       _Write32Bits
*
*   Function description
*     Sends 32 bits over SPI to SPI flash.
*/
static void _Write32Bits(U32 Data) {
  volatile U32 Dummy32;

  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif
  //
  // Transfer 32 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x3u << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI_SPDCR  &= ~0x3u;
  //
  // Start the data transfer.
  //
  RSPI_SPDR = _LoadU32BE((U8 *)&Data);
  _WaitForRxBufferReady();
  Dummy32    = RSPI_SPDR;
}

/*********************************************************************
*
*       _Write128Bits
*
*   Function description
*     Sends 32 bits over SPI to SPI flash.
*/
static void _Write128Bits(U32 * pData) {
  volatile U32 Dummy32;

  _WaitForTxBufferEmpty();
#if FS_NOR_HW_SPI_USE_OS
  _RxBufferReady = 0;
  _TxBufferEmpty = 0;
#endif // FS_NOR_HW_SPI_USE_OS
  //
  // Transfer 32 bits.
  //
  RSPI_SPCMD0 &= ~(0xFu << SPCMD_SPB);
  RSPI_SPCMD0 |=   0x3u << SPCMD_SPB;
  //
  // Perform 4 transfers (128 bits in total).
  //
  RSPI_SPDCR  |= 0x3u;
  //
  // Data transfer starts when the last word is stored to buffer.
  //
  RSPI_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI_SPDR = _LoadU32BE((U8 *)pData++);
  _WaitForRxBufferReady();
  Dummy32 = RSPI_SPDR;
  Dummy32 = RSPI_SPDR;
  Dummy32 = RSPI_SPDR;
  Dummy32 = RSPI_SPDR;
}

/*********************************************************************
*
*       _SetMaxSpeed
*/
static U16 _SetMaxSpeed(U8 Unit, U16 MaxFreq) {
  U32 SPIFreq;
  U8  RegValue;

  FS_USE_PARA(Unit);
  SPIFreq  = MaxFreq * 2;
  RegValue = (U8)((FS_NOR_HW_SPI_PER_CLOCK_KHZ + SPIFreq - 1) / SPIFreq);
  if (RegValue) {
    --RegValue;
  }
  MaxFreq = FS_NOR_HW_SPI_PER_CLOCK_KHZ / (2 * (RegValue + 1));
  //
  // RSPI must be disabled when the bit rate register is modified (see the description of SPBR in [1])
  //
  RSPI_SPCR &= ~(1u << SPCR_SPE);
  RSPI_SPBR  = RegValue;
  RSPI_SPCR |=  (1u << SPCR_SPE);
  return MaxFreq;
}

/*********************************************************************
*
*       Static code (public via callback)
*
**********************************************************************
*/

/*********************************************************************
*
*       _HW_Init
*
*  Function description
*    Initialize the SPI for use with the flash
*
*  Parameters
*    Unit   Device Index
*
*  Return value
*    SPI frequency that is set - given in kHz.
*/
static int _HW_Init(U8 Unit) {
  U16 Reg16;

  //
  // Configure the assignment of I/O ports.
  //
  MPC_PWPR   &= ~(1u << PWPR_B0WI);     // Remove write protection
  MPC_PWPR   |=   1u << PWPR_PFSWE;
  MPC_PA5PFS  = 0x0D;                   // Controlled by the RSPI peripheral
  MPC_PA6PFS  = 0x0D;                   // Controlled by the RSPI peripheral
  MPC_PA7PFS  = 0x0D;                   // Controlled by the RSPI peripheral
  MPC_PWPR   &= ~(1u << PWPR_PFSWE);    // Restore write protection
  MPC_PWPR   |=   1u << PWPR_B0WI;
  //
  // Make sure that the A4 line which is share with the CS signal is not active.
  //
  MPC_PFBCR0 &= ~(1u << PFBCR0_ADRLE);
  //
  // Configure the chip select signal as output.
  //
  PA_PDR |=   1u << NOR_CS_PIN;
  //
  // MISO is an input signal controlled by SCI.
  //
  PA_PDR &= ~(1u << NOR_MISO_PIN);
  PA_PMR |=   1u << NOR_MISO_PIN;
  //
  // MOSI is an output signal controlled by SCI.
  //
  PA_PDR |= 1u << NOR_MOSI_PIN;
  PA_PMR |= 1u << NOR_MOSI_PIN;
  //
  // CLK is an output signal controlled by SCI.
  //
  PA_PDR |= 1u << NOR_CLK_PIN;
  PA_PMR |= 1u << NOR_CLK_PIN;
  //
  // Enable the RSPI peripheral
  //
  PRCR     |= (1u << PRCR_PRC1)         // Remove write protection
           | (PRCR_MAGIC << PRCR_PRKEY)
           ;
  MSTPCRB &= ~(1u << MSTPCRB_RSPI);
  Reg16    = PRCR;
  Reg16   &= ~(1u << PRCR_PRC1);
  Reg16   |= PRCR_MAGIC << PRCR_PRKEY;
  PRCR     = Reg16;                     // Restore write protection
  //
  // Reset the RSPI peripheral.
  //
  RSPI_SPCR  = 0x0;
  //
  // Clear RSPI error flags.
  //
  RSPI_SPSR &= ~((1u << SPSR_OVRF) |
                 (1u << SPSR_MODF) |
                 (1u << SPSR_PERF));
  //
  // Single sequence using command 0 register.
  //
  RSPI_SPSCR  = 0;
  RSPI_SPDCR  = 1u << SPDCR_SPLW;       // 32-bit access to data register
  RSPI_SPCMD0 = 0
              | (1u << SPCMD_CHPA)
              | (1u << SPCMD_CPOL)
              | (7u << SPCMD_SPB)       // 8 bit transfers
              ;
  RSPI_SPCR2 = 0;
  //
  // Configure the RSSPI in master mode.
  //
  RSPI_SPCR  = (1u << SPCR_SPMS)
             | (1u << SPCR_MSTR)
             | (1u << SPCR_SPRIE)
             | (1u << SPCR_SPTIE)
             | (1u << SPCR_SPE)
             ;
  //
  // Wait for RSPI to become ready.
  //
  while (RSPI_SPSR & (1u << SPSR_ILDNF)) {
    ;
  }
#if FS_NOR_HW_SPI_USE_OS
  IPR38_SPRI1 = ISR_PRIO;
  IPR39_SPTI1 = ISR_PRIO;
#endif // FS_NOR_HW_SPI_USE_OS
  return _SetMaxSpeed(Unit, FS_NOR_HW_SPI_NOR_CLOCK_KHZ);
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Activates the SPI flash using the chip select (CS) line.
*
*  Parameters
*    Unit     Device Index
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  PA_PODR &= ~(1u << NOR_CS_PIN);         // Active low
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Deactivates the SPI flash using the chip select (CS) line.
*
*  Parameters
*    Unit     Device Index
*/
static void _HW_DisableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  PA_PODR |= 1u << NOR_CS_PIN;            // Active low
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Reads a specified number of bytes from flash to buffer.
*
*  Parameters
*    Unit       Device Index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be read
*/
static void _HW_Read(U8 Unit, U8 * pData, int NumBytes) {
  U32 NumWords;
  U32 NumBlocks;

  FS_USE_PARA(Unit);
  _ClearErrors();
  //
  // Transfer single bytes until the data pointer is 32-bit aligned.
  //
  _EnableInt();
  while (1) {
    if (NumBytes == 0) {
      break;
    }
    if (((U32)pData & 0x3) == 0) {
      break;
    }
    _Read8Bits(pData);
    ++pData;
    --NumBytes;
  }
  //
  // OK, data pointer is 32-bit aligned. Try to read 128-bit blocks at a time as this is the most efficient way to transfer data.
  //
  if (NumBytes) {
    NumBlocks = NumBytes >> 4;
    if (NumBlocks) {
      NumBytes -= NumBlocks << 4;
      do {
        _Read128Bits((U32 *)pData);
        pData += 16;
      } while (--NumBlocks);
    }
  }
  //
  // Try to read the rest of the data as words.
  //
  if (NumBytes) {
    NumWords = NumBytes >> 2;
    if (NumWords) {
      NumBytes -= NumWords << 2;
      do {
        _Read32Bits((U32 *)pData);
        pData += 4;
      } while (--NumWords);
    }
  }
  //
  // Read the remaining data as single bytes.
  //
  if (NumBytes) {
    do {
      _Read8Bits(pData);
      ++pData;
    } while (--NumBytes);
  }
  _DisableInt();
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Writes a specified number of bytes from data buffer to flash.
*
*  Parameters
*    Unit       Device Index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes) {
  U32 NumWords;
  U32 NumBlocks;

  FS_USE_PARA(Unit);
  _ClearErrors();
  //
  // Transfer single bytes until the data pointer is 32-bit aligned.
  //
  _EnableInt();
  while (1) {
    if (NumBytes == 0) {
      break;
    }
    if (((U32)pData & 0x3) == 0) {
      break;
    }
    _Write8Bits(*pData);
    ++pData;
    --NumBytes;
  }
  //
  // OK, data pointer is 32-bit aligned. Try to read 128-bit blocks at a time as this is the most efficient way to transfer data.
  //
  if (NumBytes) {
    NumBlocks = NumBytes >> 4;
    if (NumBlocks) {
      NumBytes -= NumBlocks << 4;
      do {
        _Write128Bits((U32 *)pData);
        pData += 16;
      } while (--NumBlocks);
    }
  }
  //
  // Try to write the rest of the data as words.
  //
  if (NumBytes) {
    NumWords = NumBytes >> 2;
    if (NumWords) {
      NumBytes -= NumWords << 2;
      do {
        _Write32Bits(*(U32 *)pData);
        pData += 4;
      } while (--NumWords);
    }
  }
  //
  // Write the remaining data as single bytes.
  //
  if (NumBytes) {
    do {
      _Write8Bits(*pData);
      ++pData;
    } while (--NumBytes);
  }
  _DisableInt();
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_RX65N_Renesas_RSKRX65N = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  _HW_Read,
  _HW_Write,
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
