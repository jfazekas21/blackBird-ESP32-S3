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

File    : FS_NOR_HW_SPI_RX62N_Renesas_RSKRX62N.c
Purpose : NOR SPI hardware layer for RSKRX62N Evaluation board (Rev. 02).
Literature:
  [1] RX62N Group, RX621 Group User's Manual: Hardware
    (\\fileserver\Techinfo\Company\Renesas\MCU\RX\RX62N_RX621\1407xx_RX621 RX62N HardwareUserGuide_Rev1.40.pdf)
  [2] RX62N Group Renesas Starter Kit+ User's Manual
    (\\fileserver\Techinfo\Company\Renesas\MCU\RX\EvalBoard\RSK_RX62N\RSK+RX62N_UserManual.pdf)
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
#ifndef   FS_NOR_HW_SPI_PER_CLOCK_FREQ_KHZ
  #define FS_NOR_HW_SPI_PER_CLOCK_FREQ_KHZ    48000uL   // Clock of SPI peripheral in kHz
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLOCK_FREQ_KHZ
  #define FS_NOR_HW_SPI_NOR_CLOCK_FREQ_KHZ    24000uL   // Frequency of the SPI clock.
#endif

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

//
// I/O Ports
//
#define P3_BASE_ADDR      0x0008C003
#define P3_DDR            (*(volatile EVENACCESS U8 *)(P3_BASE_ADDR + 0x00))      // Data direction register
#define P3_DR             (*(volatile EVENACCESS U8 *)(P3_BASE_ADDR + 0x20))      // Data register
#define P3_ICR            (*(volatile EVENACCESS U8 *)(P3_BASE_ADDR + 0x60))      // Input buffer control register

//
// Serial Peripheral Interface (RSPI)
//
#define RSPI1_BASE_ADDR   0x000883A0
#define RSPI1_SPCR        (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x00))  // Control register
#define RSPI1_SPSR        (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x03))  // Status register
#define RSPI1_SPDR        (*(volatile EVENACCESS U32 *)(RSPI1_BASE_ADDR + 0x04))  // Data register (32-bit)
#define RSPI1_SPSCR       (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x08))  // Sequence control register
#define RSPI1_SPBR        (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x0A))  // Bit rate register
#define RSPI1_SPDCR       (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x0B))  // Data control register
#define RSPI1_SPCR2       (*(volatile EVENACCESS U8 *) (RSPI1_BASE_ADDR + 0x0F))  // Control register 2
#define RSPI1_SPCMD0      (*(volatile EVENACCESS U16 *)(RSPI1_BASE_ADDR + 0x10))  // Command register 0

//
// Misc. registers
//
#define MSTPCRB           (*(volatile EVENACCESS U32 *)0x00080014)                // Module stop control register B
#define PFHSPI            (*(volatile EVENACCESS U8  *)0x0008C111)                // Port function control register H

//
// RSPI port function control bits
//
#define PFGSPI_RSPCKE     1
#define PFGSPI_MOSIE      2
#define PFGSPI_MISOE      3

//
// RSPI control register bits
//
#define SPCR_SPMS         0
#define SPCR_MODFEN       2
#define SPCR_MSTR         3
#define SPCR_SPE          6

//
// RSPI status register bits
//
#define SPSR_OVRF         0
#define SPSR_ILDNF        1
#define SPSR_MODF         2
#define SPSR_PERF         3
#define SPSR_SPTEF        5
#define SPSR_SPRF         7

//
// RSPI data control register bits
//
#define SPDCR_SPLW        5

//
// RSPI command register bits
//
#define SPCMD_CHPA        0
#define SPCMD_CPOL        1
#define SPCMD_SPB         8

//
// Misc. bit defines
//
#define SD_MISO_PIN       0   // Master in slave out (P3)
#define SD_CS_PIN         1   // Chip select (P3)
#define MSTPCRB_RSPI1     16  // Start/Stop RSPI1

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _LoadU32BE
*
*  Function description
*    Reads a 32 bit value stored in big endian format from a byte array.
*/
static U32 _LoadU32BE(const U8 * pBuffer) {
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
*     Performs error recovery. By now only the receiver overrun error is handled.
*/
static void _ClearErrors(void) {
  volatile U32 Data;

  //
  // Clear overrun error if required. Use the same sequence as the one described in section "32.3.14 Error Handling" of [1]
  //
  if (RSPI1_SPSR & (1 << SPSR_OVRF)) {
    RSPI1_SPSR &= ~(1 << SPSR_OVRF);
    Data        = RSPI1_SPDR;
    if (RSPI1_SPSR & (1 << SPSR_SPTEF)) {    // Transmit buffer not empty ?
      RSPI1_SPCR &= ~(1 << SPCR_SPE);
      RSPI1_SPCR |=  (1 << SPCR_SPE);
    }
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
  while ((RSPI1_SPSR & (1 << SPSR_SPTEF)) == 0) {   // Wait for data to be transmitted.
    ;
  }
}

/*********************************************************************
*
*       _WaitForRxBufferReady
*
*   Function description
*     Waits for RSPI to fill the receive buffer with data.
*/
static void _WaitForRxBufferReady(void) {
  while ((RSPI1_SPSR & (1 << SPSR_SPRF)) == 0) {    // Wait for data to be received.
    ;
  }
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
  //
  // Transfer 8 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x7 << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI1_SPDCR  &= ~0x3;
  //
  // Start the data transfer.
  //
  RSPI1_SPDR    = 0xFF;
  _WaitForRxBufferReady();
  //
  // Store data to receive buffer.
  //
  *pData = (U8)RSPI1_SPDR;
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
  //
  // Transfer 32 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x3 << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI1_SPDCR  &= ~0x3;
  //
  // Start the data transfer.
  //
  RSPI1_SPDR = 0xFFFFFFFF;
  _WaitForRxBufferReady();
  Data32   = RSPI1_SPDR;
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
  //
  // Transfer 32 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x3 << SPCMD_SPB;
  //
  // Perform 4 transfers (128 bits in total).
  //
  RSPI1_SPDCR  |= 0x3;
  //
  // Data transfer starts when the last word is stored to buffer.
  //
  RSPI1_SPDR = 0xFFFFFFFF;
  RSPI1_SPDR = 0xFFFFFFFF;
  RSPI1_SPDR = 0xFFFFFFFF;
  RSPI1_SPDR = 0xFFFFFFFF;
  _WaitForRxBufferReady();
  Data32   = RSPI1_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI1_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI1_SPDR;
  *pData++ = _LoadU32BE((U8 *)&Data32);
  Data32   = RSPI1_SPDR;
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
  //
  // Transfer 8 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x7 << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI1_SPDCR  &= ~0x3;
  //
  // Start the data transfer.
  //
  RSPI1_SPDR    = (U32)Data;
  _WaitForRxBufferReady();
  //
  // Read data from buffer to prevent a receive overrun error.
  //
  Dummy32 = RSPI1_SPDR;
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
  //
  // Transfer 32 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x3 << SPCMD_SPB;
  //
  // Perform 1 transfer.
  //
  RSPI1_SPDCR  &= ~0x3;
  //
  // Start the data transfer.
  //
  RSPI1_SPDR = _LoadU32BE((U8 *)&Data);
  _WaitForRxBufferReady();
  Dummy32    = RSPI1_SPDR;
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
  //
  // Transfer 32 bits.
  //
  RSPI1_SPCMD0 &= ~(0xF << SPCMD_SPB);
  RSPI1_SPCMD0 |=   0x3 << SPCMD_SPB;
  //
  // Perform 4 transfers (128 bits in total).
  //
  RSPI1_SPDCR  |= 0x3;
  //
  // Data transfer starts when the last word is stored to buffer.
  //
  RSPI1_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI1_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI1_SPDR = _LoadU32BE((U8 *)pData++);
  RSPI1_SPDR = _LoadU32BE((U8 *)pData++);
  _WaitForRxBufferReady();
  Dummy32 = RSPI1_SPDR;
  Dummy32 = RSPI1_SPDR;
  Dummy32 = RSPI1_SPDR;
  Dummy32 = RSPI1_SPDR;
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
  RegValue = (U8)((FS_NOR_HW_SPI_PER_CLOCK_FREQ_KHZ + SPIFreq - 1) / SPIFreq);
  if (RegValue) {
    --RegValue;
  }
  MaxFreq   = FS_NOR_HW_SPI_PER_CLOCK_FREQ_KHZ / (2 * (RegValue + 1));
  //
  // RSPI must be disabled when the bit rate register is modified (see the description of SPBR in [1])
  //
  RSPI1_SPCR &= ~(1 << SPCR_SPE);
  RSPI1_SPBR  = RegValue;
  RSPI1_SPCR |=  (1 << SPCR_SPE);
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
*    Initialize the SPI for use with the NOR flash.
*
*  Parameters
*    Unit   Device index
*
*  Return value
*    SPI frequency that is set - given in kHz, 0 in case of an error.
*/
static int _HW_Init(U8 Unit) {
  //
  // Configure the chip select signal as output and enable the input buffer of MISO signal.
  //
  P3_DDR |= (1 << SD_CS_PIN);
  P3_ICR |= (1 << SD_MISO_PIN);
  //
  // The SPI flash is connected on port 2 and 3. The chip select signal is controlled by the driver.
  //
  PFHSPI = (1 << PFGSPI_RSPCKE)
         | (1 << PFGSPI_MOSIE)
         | (1 << PFGSPI_MISOE)
         ;

  //
  // Enable the RSPI peripheral
  //
  MSTPCRB    &= ~(1u << MSTPCRB_RSPI1);
  //
  // Reset the RSPI peripheral.
  //
  RSPI1_SPCR  = 0x0;
  //
  // Clear RSPI error flags.
  //
  RSPI1_SPSR &= ~((1 << SPSR_OVRF) |
                  (1 << SPSR_MODF) |
                  (1 << SPSR_PERF));
  //
  // Single sequence using command 0 register.
  //
  RSPI1_SPSCR  = 0;
  RSPI1_SPDCR  = 1 << SPDCR_SPLW;     // 32-bit access to data register
  RSPI1_SPCMD0 = 0
               | (1 << SPCMD_CHPA)
               | (1 << SPCMD_CPOL)
               | (7 << SPCMD_SPB)     // 8 bit transfers
               ;
  RSPI1_SPCR2 = 0;
  //
  // Configure the RSSPI in master mode.
  //
  RSPI1_SPCR  = (1 << SPCR_SPMS)
              | (1 << SPCR_MSTR)
              | (1 << SPCR_SPE)
              ;
  //
  // Wait for RSPI to become ready.
  //
  while (RSPI1_SPSR & (1 << SPSR_ILDNF)) {
    ;
  }
  return _SetMaxSpeed(Unit, FS_NOR_HW_SPI_NOR_CLOCK_FREQ_KHZ);
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
  P3_DR &= ~(1 << SD_CS_PIN);           // Active low
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
  P3_DR |= 1 << SD_CS_PIN;              // Active low
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
  U32 NumWords;
  U32 NumBlocks;

  FS_USE_PARA(Unit);
  _ClearErrors();
  //
  // Transfer single bytes until the data pointer is 32-bit aligned.
  //
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
  U32 NumWords;
  U32 NumBlocks;

  FS_USE_PARA(Unit);
  _ClearErrors();
  //
  // Transfer single bytes until the data pointer is 32-bit aligned.
  //
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
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_RX62N_Renesas_RSKRX62N = {
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
