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

File    : FS_NOR_HW_SPI_K66_SEGGER_emPower.c
Purpose : NOR SPI hardware layer for the SEGGER emPower board
Literature:
  [1] K66 Sub-Family Reference Manual
      (\\FILESERVER\Techinfo\Company\Freescale\MCU\Kinetis_K-series\K66P144M180SF5RMV2_Rev2_1505.pdf)
  [2] emPower Evaluation and prototyping platform for SEGGER software User Guide & Reference Manual
      (\\FILESERVER\Product\Doc4Review\UM06001_emPower.pdf)
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
#ifndef   FS_NOR_HW_SPI_BUS_CLK_HZ
  #define FS_NOR_HW_SPI_BUS_CLK_HZ          (168000000uL / 3)   // SPI module clock
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLK_HZ
  #define FS_NOR_HW_SPI_NOR_CLK_HZ          28000000uL          // Clock of NOR flash device
#endif

#ifndef   FS_NOR_HW_SPI_INIT_WAIT_LOOPS
  #define FS_NOR_HW_SPI_INIT_WAIT_LOOPS     20000               // Waits about 1ms for the SPI module to start/stop
#endif

#ifndef   FS_NOR_HW_SPI_UNIT_NO
  #define FS_NOR_HW_SPI_UNIT_NO             0                   // Selects the number of SPI unit to be used for data transfer (Permitted values: 0 and 2)
#endif

#ifndef   FS_NOR_HW_SPI_USE_OS
  #define FS_NOR_HW_SPI_USE_OS              0                   // Enables / disables the event-driven operation.
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_SPI_USE_OS
  #include "FS_OS.h"
  #include "RTOS.h"
  #include "MK66F18.h"
#endif // FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       SPI module
*/
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
  #define SPI_BASE_ADDR     0x400AC000uL
#else
  #define SPI_BASE_ADDR     0x4002C000uL
#endif
#define SPI_MCR             (*(volatile U32 *)(SPI_BASE_ADDR + 0x00))     // Module Configuration Register
#define SPI_TCR             (*(volatile U32 *)(SPI_BASE_ADDR + 0x08))     // Transfer Count Register
#define SPI_CTAR0           (*(volatile U32 *)(SPI_BASE_ADDR + 0x0C))     // Clock and Transfer Attributes Register (In Master Mode)
#define SPI_CTAR1           (*(volatile U32 *)(SPI_BASE_ADDR + 0x10))     // Clock and Transfer Attributes Register (In Master Mode)
#define SPI_SR              (*(volatile U32 *)(SPI_BASE_ADDR + 0x2C))     // Status Register
#define SPI_RSER            (*(volatile U32 *)(SPI_BASE_ADDR + 0x30))     // DMA/Interrupt Request Select and Enable Register
#define SPI_PUSHR           (*(volatile U32 *)(SPI_BASE_ADDR + 0x34))     // PUSH TX FIFO Register In Master Mode
#define SPI_POPR            (*(volatile U32 *)(SPI_BASE_ADDR + 0x38))     // POP RX FIFO Register
#define SPI_TXFR0           (*(volatile U32 *)(SPI_BASE_ADDR + 0x3C))     // Transmit FIFO Registers
#define SPI_TXFR1           (*(volatile U32 *)(SPI_BASE_ADDR + 0x40))     // Transmit FIFO Registers
#define SPI_TXFR2           (*(volatile U32 *)(SPI_BASE_ADDR + 0x44))     // Transmit FIFO Registers
#define SPI_TXFR3           (*(volatile U32 *)(SPI_BASE_ADDR + 0x48))     // Transmit FIFO Registers
#define SPI_RXFR0           (*(volatile U32 *)(SPI_BASE_ADDR + 0x7C))     // Receive FIFO Registers
#define SPI_RXFR1           (*(volatile U32 *)(SPI_BASE_ADDR + 0x80))     // Receive FIFO Registers
#define SPI_RXFR2           (*(volatile U32 *)(SPI_BASE_ADDR + 0x84))     // Receive FIFO Registers
#define SPI_RXFR3           (*(volatile U32 *)(SPI_BASE_ADDR + 0x88))     // Receive FIFO Registers

#if (FS_NOR_HW_SPI_USE_OS == 0)

/*********************************************************************
*
*       System integration module
*/
#define SIM_BASE_ADDR       0x40047000uL
#define SIM_SCGC3           (*(volatile U32 *)(SIM_BASE_ADDR + 0x1030))   // System Clock Gating Control Register 3
#define SIM_SCGC5           (*(volatile U32 *)(SIM_BASE_ADDR + 0x1038))   // System Clock Gating Control Register 5
#define SIM_SCGC6           (*(volatile U32 *)(SIM_BASE_ADDR + 0x103C))   // System Clock Gating Control Register 6

/*********************************************************************
*
*       Port B
*/
#define PORTB_BASE_ADDR     0x4004A000uL
#define PORTB_PCR20         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x50))   // Port Control Register
#define PORTB_PCR21         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x54))   // Port Control Register
#define PORTB_PCR22         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x58))   // Port Control Register
#define PORTB_PCR23         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x5C))   // Port Control Register
#define PORTB_DFER          (*(volatile U32 *)(PORTB_BASE_ADDR + 0xC0))   // Digital filter enable register

/*********************************************************************
*
*       Port C
*/
#define PORTC_BASE_ADDR     0x4004B000uL
#define PORTC_PCR4          (*(volatile U32 *)(PORTC_BASE_ADDR + 0x10))   // Port Control Register
#define PORTC_PCR5          (*(volatile U32 *)(PORTC_BASE_ADDR + 0x14))   // Port Control Register
#define PORTC_PCR6          (*(volatile U32 *)(PORTC_BASE_ADDR + 0x18))   // Port Control Register
#define PORTC_PCR7          (*(volatile U32 *)(PORTC_BASE_ADDR + 0x1C))   // Port Control Register
#define PORTC_DFER          (*(volatile U32* )(PORTC_BASE_ADDR + 0xC0))   // Digital filter enable register

/*********************************************************************
*
*       GPIO B
*/
#define GPIOB_BASE_ADDR     0x400FF040uL
#define GPIOB_PDOR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))   // Port Data Output Register
#define GPIOB_PSOR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x04))   // Port Set Output Register
#define GPIOB_PCOR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x08))   // Port Clear Output Register
#define GPIOB_PTOR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x0C))   // Port Toggle Output Register
#define GPIOB_PDIR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x10))   // Port Data Input Register
#define GPIOB_PDDR          (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x14))   // Port Data Direction Register

/*********************************************************************
*
*       GPIO C
*/
#define GPIOC_BASE_ADDR     0x400FF080uL
#define GPIOC_PDOR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x00))   // Port Data Output Register
#define GPIOC_PSOR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x04))   // Port Set Output Register
#define GPIOC_PCOR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x08))   // Port Clear Output Register
#define GPIOC_PTOR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x0C))   // Port Toggle Output Register
#define GPIOC_PDIR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x10))   // Port Data Input Register
#define GPIOC_PDDR          (*(volatile U32 *)(GPIOC_BASE_ADDR + 0x14))   // Port Data Direction Register

#else

/*********************************************************************
*
*       Port C
*/
#define PORTC_BASE_ADDR     0x4004B000uL
#define PORTC_DFER          (*(volatile U32* )(PORTC_BASE_ADDR + 0xC0))   // Digital filter enable register

#endif // FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       Port Control Register
*/
#define PCR_MUX_BIT         8     // Alternate port setting
#define PCR_MUX_GPIO        1uL   // Controlled directly by the HW layer
#define PCR_MUX_SPI0        2uL   // Controlled by SPI module
#define PCR_MUX_SPI2        2uL   // Controlled by SPI module
#define PCR_MUX_MASK        0x7uL
#define PCR_DSE_BIT         6     // Driver strength enable
#define PCR_PE_BIT          1     // Pull enable
#define PCR_PS_BIT          0     // Pull-up enable

/*********************************************************************
*
*       Clock enable bits
*/
#define SCGC3_SPI2_BIT      12  // SPI2
#define SCGC5_PORTB_BIT     10  // GPIO B
#define SCGC5_PORTC_BIT     11  // GPIO C
#define SCGC6_SPI0_BIT      12  // SPI0

/*********************************************************************
*
*       SPI Module Configuration Register
*/
#define MCR_MSTR_BIT        31
#define MCR_FRZ_BIT         27
#define MCR_CLR_TXF_BIT     11
#define MCR_CLR_RXF_BIT     10
#define MCR_HALT_BIT        0

/*********************************************************************
*
*       SPI Status Register
*/
#define SR_TCF_BIT          31
#define SR_TXRXS_BIT        30
#define SR_EOQF_BIT         28
#define SR_TFUF_BIT         27
#define SR_TFFF_BIT         25
#define SR_RFOF_BIT         19
#define SR_RFDF_BIT         17
#define SR_RXCTR_BIT        4
#define SR_RXCTR_MASK       0xFuL

/*********************************************************************
*
*       SPI Clock and Transfer Attributes Register
*/
#define CTAR_DBR_BIT        31
#define CTAR_FMSZ_BIT       27
#define CTAR_CPOL_BIT       26
#define CTAR_CPHA_BIT       25
#define CTAR_BR_BIT         0
#define CTAR_BR_MAX         0xFuL

/*********************************************************************
*
*       SPI misc. defines
*/
#define PUSHR_CTAS_BIT      28
#define RSER_RFDF_RE_BIT    17
#define SPI_IRQ_PRIO        15
#define WAIT_TIMEOUT_MS     1000

/*********************************************************************
*
*       NOR flash pin assignments
*/
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
  #define NOR_CS_PIN        20  // Port B
  #define NOR_CLK_PIN       21  // Port B
  #define NOR_MOSI_PIN      22  // Port B
  #define NOR_MISO_PIN      23  // Port B
#else
  #define NOR_CS_PIN        4   // Port C
  #define NOR_CLK_PIN       5   // Port C
  #define NOR_MOSI_PIN      6   // Port C
  #define NOR_MISO_PIN      7   // Port C
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _StoreU16BE
*
*  Function description
*    Stores a 16 bit value in big endian format into a byte array.
*/
static void _StoreU16BE(U8 *pBuffer, unsigned Data) {
  *pBuffer++ = (U8)(Data >> 8);
  *pBuffer   = (U8) Data;
}

/*********************************************************************
*
*       _LoadU16BE
*
*  Function description
*    Reads a 16 bit value stored in big endian format from a byte array.
*/
static U16 _LoadU16BE(const U8 * pBuffer) {
  U16 r;

  r = *pBuffer++;
  r = (U16)((r << 8) | *pBuffer);
  return r;
}

/*********************************************************************
*
*       _Transfer8Bit
*/
static U8 _Transfer8Bit(U8 Data) {
  U32 NumBytes;

  SPI_PUSHR = Data;
  //
  // Wait to receive 1 byte.
  //
  while (1) {
    NumBytes = (SPI_SR >> SR_RXCTR_BIT) & SR_RXCTR_MASK;
    if (NumBytes == 1) {
      break;
    }
#if FS_NOR_HW_SPI_USE_OS
    {
      int r;

      SPI_RSER |= 1uL << RSER_RFDF_RE_BIT;     // Enable the interrupt.
      r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
      if (r != 0) {
        break;
      }
    }
#endif // FS_NOR_HW_SPI_USE_OS
  }
  Data = (U8)SPI_POPR;
  return Data;
}

#if (FS_NOR_HW_SPI_UNIT_NO == 0)

/*********************************************************************
*
*       _Read64Bits
*
*  Function description
*    Transfers 64 bits of data from NOR device to MCU.
*/
static void _Read64Bits(U8 * pData) {
  U16 Data16;
  U32 NumItems;
  U32 Dummy;

  //
  // Send dummy data.
  //
  Dummy = 0
        | 0xFFFFuL
        | (1uL << PUSHR_CTAS_BIT)             // Use the second set of parameters which is configured for 16-bit transfers.
        ;
  SPI_PUSHR = Dummy;
  SPI_PUSHR = Dummy;
  SPI_PUSHR = Dummy;
  SPI_PUSHR = Dummy;
  //
  // Wait to receive 4 half-words (4 is the FIFO size)
  //
  while (1) {
    NumItems = (SPI_SR >> SR_RXCTR_BIT) & SR_RXCTR_MASK;
    if (NumItems == 4) {
      break;
    }
#if FS_NOR_HW_SPI_USE_OS
    {
      int r;

      SPI_RSER |= 1uL << RSER_RFDF_RE_BIT;     // Enable the interrupt.
      r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
      if (r != 0) {
        break;
      }
    }
#endif // FS_NOR_HW_SPI_USE_OS
  }
  //
  // Read data from FIFO.
  //
  Data16 = (U16)SPI_POPR;
  _StoreU16BE(pData, Data16);
  Data16 = (U16)SPI_POPR;
  _StoreU16BE(pData + 2, Data16);
  Data16 = (U16)SPI_POPR;
  _StoreU16BE(pData + 4, Data16);
  Data16 = (U16)SPI_POPR;
  _StoreU16BE(pData + 6, Data16);
}

/*********************************************************************
*
*       _Write64Bits
*
*  Function description
*    Transfers 64 bits of data from MCU to NOR device.
*/
static void _Write64Bits(const U8 * pData) {
  U16 Data16;
  U32 NumItems;

  //
  // Write data to NOR device. Use the second set of parameters which is configured for 16-bit transfers.
  //
  Data16 = _LoadU16BE(pData);
  SPI_PUSHR = (U32)Data16 | (1uL << PUSHR_CTAS_BIT);
  Data16 = _LoadU16BE(pData + 2);
  SPI_PUSHR = (U32)Data16 | (1uL << PUSHR_CTAS_BIT);
  Data16 = _LoadU16BE(pData + 4);
  SPI_PUSHR = (U32)Data16 | (1uL << PUSHR_CTAS_BIT);
  Data16 = _LoadU16BE(pData + 6);
  SPI_PUSHR = (U32)Data16 | (1uL << PUSHR_CTAS_BIT);
  //
  // Wait to receive 4 half-words (4 is the FIFO size)
  //
  while (1) {
    NumItems = (SPI_SR >> SR_RXCTR_BIT) & SR_RXCTR_MASK;
    if (NumItems == 4) {
      break;
    }
#if FS_NOR_HW_SPI_USE_OS
    {
      int r;

      SPI_RSER |= 1uL << RSER_RFDF_RE_BIT;     // Enable the interrupt.
      r = FS_X_OS_Wait(WAIT_TIMEOUT_MS);
      if (r != 0) {
        break;
      }
    }
#endif // FS_NOR_HW_SPI_USE_OS
  }
  //
  // Discard the received data.
  //
  SPI_MCR |= 1uL << MCR_CLR_RXF_BIT;
}

#endif // FS_NOR_HW_SPI_UNIT_NO == 0

/*********************************************************************
*
*       Public code (via callback)
*
**********************************************************************
*/

/*********************************************************************
*
*       _HW_Init
*
*  Function description
*    Initialize the SPI for use with the NOR flash.
*
*  Parameters
*    Unit   Device index
*
*  Return value
*    SPI frequency that is set - given in kHz, 0 in case of an error.
*/
static int _HW_Init(U8 Unit) {
  int      Freq_Hz;
  unsigned NumLoops;
  unsigned Scaler;
  unsigned Div;

  FS_USE_PARA(Unit);
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
  //
  // Enable the clock of GPIOs and SPI0.
  //
  SIM_SCGC3 |= 1uL << SCGC3_SPI2_BIT;
  SIM_SCGC5 |= 1uL << SCGC5_PORTB_BIT;
  //
  // Clock line (controlled by the SPI module)
  //
  PORTB_PCR21 = 0
              | (PCR_MUX_SPI2 << PCR_MUX_BIT)
              | (1uL          << PCR_PE_BIT)
              | (1uL          << PCR_PS_BIT)
              | (1uL          << PCR_DSE_BIT)
              ;
  //
  // MOSI line (controlled by the SPI module)
  //
  PORTB_PCR22 = 0
              | (PCR_MUX_SPI2 << PCR_MUX_BIT)
              | (1uL          << PCR_PE_BIT)
              | (1uL          << PCR_PS_BIT)
              | (1uL          << PCR_DSE_BIT)
              ;
  //
  // MISO line (controlled by the SPI module)
  //
  PORTB_PCR23 = 0
              | (PCR_MUX_SPI2 << PCR_MUX_BIT)
              | (1uL          << PCR_PE_BIT)
              | (1uL          << PCR_PS_BIT)
              | (1uL          << PCR_DSE_BIT)
              ;
  //
  // Chip select (controlled by the HW layer)
  //
  PORTB_PCR20 = 0
              | (PCR_MUX_GPIO << PCR_MUX_BIT)
              | (1uL          << PCR_PE_BIT)
              | (1uL          << PCR_PS_BIT)
              ;
  GPIOB_PSOR  = 1uL << NOR_CS_PIN;         // Active low
  GPIOB_PDDR |= 1uL << NOR_CS_PIN;
  //
  // Disable the digital filtering on all port pins controlled by the SPI module.
  //
  PORTB_DFER &= ~((1uL << NOR_CLK_PIN)  |
                  (1uL << NOR_MISO_PIN) |
                  (1uL << NOR_MOSI_PIN));
#else
  //
  // Enable the clock of GPIOs and SPI0.
  //
  SIM_SCGC5 |= 1uL << SCGC5_PORTC_BIT;
  SIM_SCGC6 |= 1uL << SCGC6_SPI0_BIT;
  //
  // Clock line (controlled by the SPI module)
  //
  PORTC_PCR5 = 0
             | (PCR_MUX_SPI0 << PCR_MUX_BIT)
             | (1uL          << PCR_PE_BIT)
             | (1uL          << PCR_PS_BIT)
             | (1uL          << PCR_DSE_BIT)
             ;
  //
  // MOSI line (controlled by the SPI module)
  //
  PORTC_PCR6 = 0
             | (PCR_MUX_SPI0 << PCR_MUX_BIT)
             | (1uL          << PCR_PE_BIT)
             | (1uL          << PCR_PS_BIT)
             | (1uL          << PCR_DSE_BIT)
             ;
  //
  // MISO line (controlled by the SPI module)
  //
  PORTC_PCR7 = 0
             | (PCR_MUX_SPI0 << PCR_MUX_BIT)
             | (1uL          << PCR_PE_BIT)
             | (1uL          << PCR_PS_BIT)
             | (1uL          << PCR_DSE_BIT)
             ;
  //
  // Chip select (controlled by the HW layer)
  //
  PORTC_PCR4 = 0
             | (PCR_MUX_GPIO << PCR_MUX_BIT)
             | (1uL          << PCR_PE_BIT)
             | (1uL          << PCR_PS_BIT)
             ;
  GPIOC_PSOR  = 1uL << NOR_CS_PIN;         // Active low
  GPIOC_PDDR |= 1uL << NOR_CS_PIN;
  //
  // Disable the digital filtering on all port pins controlled by the SPI module.
  //
  PORTC_DFER &= ~((1uL << NOR_CLK_PIN)  |
                  (1uL << NOR_MISO_PIN) |
                  (1uL << NOR_MOSI_PIN));
#endif // FS_NOR_HW_SPI_UNIT_NO == 2
  //
  // Disable the interrupt and configure the priority.
  //
#if FS_NOR_HW_SPI_USE_OS
  NVIC_DisableIRQ(SPI0_IRQn);
  NVIC_SetPriority(SPI0_IRQn, SPI_IRQ_PRIO);
#endif // FS_NOR_HW_SPI_USE_OS
  //
  // Calculate the clock scaler based on the configured clock frequency.
  //
  Scaler  = 0;
  Div     = 2;
  Freq_Hz = FS_NOR_HW_SPI_BUS_CLK_HZ;
  while (1) {
    if (((unsigned)Freq_Hz / Div) <= FS_NOR_HW_SPI_NOR_CLK_HZ) {
      break;
    }
    //
    // We do this here since the 3rd divisor is not power of 2.
    //
    if (Div == 2) {
      Div = 4;
    } else if (Div == 4) {
      Div = 6;
    } else if (Div == 6) {
      Div = 8;
    } else {
      Div <<= 1;
    }
    ++Scaler;
    if (Scaler == CTAR_BR_MAX) {
      break;
    }
  }
  //
  // Stop the SPI module.
  //
  SPI_MCR |= 1uL << MCR_HALT_BIT;
  NumLoops = FS_NOR_HW_SPI_INIT_WAIT_LOOPS;
  do {
    if ((SPI_SR & (1uL << SR_TXRXS_BIT)) == 0) {
      break;
    }
  } while (--NumLoops);
  //
  // Configure the SPI module.
  //
  SPI_MCR   = 0
            | (1uL << MCR_MSTR_BIT)
            | (1uL << MCR_CLR_TXF_BIT)
            | (1uL << MCR_CLR_RXF_BIT)
            | (1uL << MCR_HALT_BIT)
            ;
  SPI_TCR   = 0;
  SPI_CTAR0 = 0
            | (1uL    << CTAR_DBR_BIT)
            | (7uL    << CTAR_FMSZ_BIT)     // 8 bits
            | (1uL    << CTAR_CPOL_BIT)
            | (1uL    << CTAR_CPHA_BIT)
            | (Scaler << CTAR_BR_BIT)
            ;
  SPI_CTAR1 = 0
            | (1uL    << CTAR_DBR_BIT)
            | (15uL   << CTAR_FMSZ_BIT)     // 16 bits
            | (1uL    << CTAR_CPOL_BIT)
            | (1uL    << CTAR_CPHA_BIT)
            | (Scaler << CTAR_BR_BIT)
            ;
  SPI_SR    = 0
            | (1uL << SR_TCF_BIT)
            | (1uL << SR_EOQF_BIT)
            | (1uL << SR_TFUF_BIT)
            | (1uL << SR_TFFF_BIT)
            | (1uL << SR_RFOF_BIT)
            | (1uL << SR_RFDF_BIT)
            ;
  //
  // Start the SPI module.
  //
  SPI_MCR &= ~(1uL << MCR_HALT_BIT);
  NumLoops = FS_NOR_HW_SPI_INIT_WAIT_LOOPS;
  do {
    if (SPI_SR & (1uL << SR_TXRXS_BIT)) {
      break;
    }
  } while (--NumLoops);
  //
  // Make a dummy transfer so that the polarity of the clock signal is set correctly.
  // Without this, the first byte transfer will send/receive the wrong value.
  //
  _Transfer8Bit(0xFF);
#if FS_NOR_HW_SPI_USE_OS
  NVIC_EnableIRQ(SPI0_IRQn);
#else
  SPI_RSER  = 0;
#endif
  return Freq_Hz;
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Activates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit   Device index
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
  GPIOB_PCOR = 1uL << NOR_CS_PIN;         // Active low
#else
  GPIOC_PCOR = 1uL << NOR_CS_PIN;         // Active low
#endif
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Deactivates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit   Device index
*/
static void _HW_DisableCS(U8 Unit) {
  FS_USE_PARA(Unit);
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
  GPIOB_PSOR = 1uL << NOR_CS_PIN;         // Active low
#else
  GPIOC_PSOR = 1uL << NOR_CS_PIN;         // Active low
#endif
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Reads a specified number of bytes from NOR flash to buffer.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be read
*/
static void _HW_Read(U8 Unit, U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  if (NumBytes) {
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
    do {
      *pData++ = _Transfer8Bit(0xFF);
    } while (--NumBytes);
#else
    //
    // Read 64 bits at a time if possible.
    //
    if (((unsigned)NumBytes & 7) == 0) {      // Multiple of 8 bytes?
      unsigned NumLoops;

      NumLoops = (unsigned)NumBytes >> 3;
      do {
        _Read64Bits(pData);
        pData += 8;
      } while (--NumLoops);
    } else {
      do {
        *pData++ = _Transfer8Bit(0xFF);
      } while (--NumBytes);
    }
#endif
  }
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Writes a specified number of bytes from data buffer to NOR flash.
*
*  Parameters
*    Unit       Device index
*    pData      Pointer to a data buffer
*    NumBytes   Number of bytes to be written
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes) {
  FS_USE_PARA(Unit);
  if (NumBytes) {
#if (FS_NOR_HW_SPI_UNIT_NO == 2)
    do {
      (void)_Transfer8Bit(*pData++);
    } while (--NumBytes);
#else
    //
    // Write 64 bits at a time if possible.
    //
    if (((unsigned)NumBytes & 7) == 0) {      // Multiple of 8 bytes?
      unsigned NumLoops;

      NumLoops = (unsigned)NumBytes >> 3;
      do {
        _Write64Bits(pData);
        pData += 8;
      } while (--NumLoops);
    } else {
      do {
        (void)_Transfer8Bit(*pData++);
      } while (--NumBytes);
    }
#endif
  }
}

/*********************************************************************
*
*      Public code
*
**********************************************************************
*/

#if FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       SPI0_IRQHandler
*/
void SPI0_IRQHandler(void);
void SPI0_IRQHandler(void) {
  OS_EnterInterrupt();
  SPI_RSER &= ~(1uL << RSER_RFDF_RE_BIT);       // Disable the interrupt.
  FS_X_OS_Signal();
  OS_LeaveInterrupt();
}

#endif // FS_NOR_HW_SPI_USE_OS

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_K66_SEGGER_emPower = {
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
  NULL,
  NULL
};

/*************************** End of file ****************************/
