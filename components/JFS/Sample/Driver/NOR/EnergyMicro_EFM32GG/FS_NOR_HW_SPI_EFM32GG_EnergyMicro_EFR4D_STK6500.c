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

File:    FS_NOR_HW_SPI_EFM32GG_EnergyMicro_EFR4D_STK6500.c
Purpose: Sample NOR SPI hardware layer for Energy Micro EFR4D-STK6500 evaluation board
Literature:
  [1] EFR4D Wireless Starter Kit - Schematics
    (\\FILESERVER\Techinfo\Company\EnergyMicro\Evalboard\EFR-STK\BRD4000A.pdf)
  [2] EFR4D-STK6500 Prototype - Getting Started Guide
    (\\FILESERVER\Techinfo\Company\EnergyMicro\Evalboard\EFR-STK\EFRSTK_getting_started.pdf)
  [3] EFM32GG Reference Manual
    (\\fileserver\Techinfo\Company\EnergyMicro\GiantGecko\d0053_EFM32GG_TRM_2012-04-24_Rev0.96.pdf)
  [4] EFM32GG990 DATASHEET
    (\\FILESERVER\Techinfo\Company\EnergyMicro\GiantGecko\d0046_efm32gg990_datasheet.pdf)
*/

/*********************************************************************
*
*       #include Section
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
#ifndef   FS_NOR_HW_SPI_PER_CLK_HZ
  #define FS_NOR_HW_SPI_PER_CLK_HZ        48000000uL  // Input clock of USART
#endif

#ifndef   FS_NOR_HW_SPI_NOR_CLK_HZ
  #define FS_NOR_HW_SPI_NOR_CLK_HZ        24000000uL  // Speed of the SPI clock supplied to NOR flash device
#endif

#ifndef   FS_NOR_HW_SPI_USE_DMA
  #define FS_NOR_HW_SPI_USE_DMA           1           // Enables/disables the support for the data transfer via DMA
#endif

#ifndef   FS_NOR_HW_SPI_DMA_CHANNEL_TX
  #define FS_NOR_HW_SPI_DMA_CHANNEL_TX    0           // Index of DMA channel that sends data to NOR flash device
#endif

#ifndef   FS_NOR_HW_SPI_DMA_CHANNEL_RX
  #define FS_NOR_HW_SPI_DMA_CHANNEL_RX    1           // Index of DMA channel that receives data from NOR flash device
#endif

/*********************************************************************
*
*       Defines, non-configurable
*
**********************************************************************
*/

/*********************************************************************
*
*       General Purpose I/O
*/
#define GPIO_BASE_ADDR                  0x40006000uL
#define GPIOE_MODEL                     (*(volatile U32*) (GPIO_BASE_ADDR + 0x094))    // Port Pin Mode Low Register
#define GPIOE_DOUTSET                   (*(volatile U32*) (GPIO_BASE_ADDR + 0x0A0))    // Port Data Out Set Register
#define GPIOE_DOUTCLR                   (*(volatile U32*) (GPIO_BASE_ADDR + 0x0A4))    // Port Data Out Clear Register

/*********************************************************************
*
*       USART0
*/
#define USART0_BASE_ADDR                0x4000C000uL
#define USART0_CTRL                     (*(volatile U32*) (USART0_BASE_ADDR + 0x000))  // Control Register
#define USART0_CMD                      (*(volatile U32*) (USART0_BASE_ADDR + 0x00C))  // Command Register
#define USART0_STATUS                   (*(volatile U32*) (USART0_BASE_ADDR + 0x010))  // USART Status Register
#define USART0_CLKDIV                   (*(volatile U32*) (USART0_BASE_ADDR + 0x014))  // Clock Control Register
#define USART0_RXDATA                   (*(volatile U32*) (USART0_BASE_ADDR + 0x01C))  // RX Buffer Data Register
#define USART0_TXDATA                   (*(volatile U32*) (USART0_BASE_ADDR + 0x034))  // TX Buffer Data Register
#define USART0_IFC                      (*(volatile U32*) (USART0_BASE_ADDR + 0x048))  // Interrupt Flag Clear Register
#define USART0_IEN                      (*(volatile U32*) (USART0_BASE_ADDR + 0x04C))  // Interrupt Enable Register
#define USART0_ROUTE                    (*(volatile U32*) (USART0_BASE_ADDR + 0x054))  // I/O Routing Register

/*********************************************************************
*
*       Clock management unit
*/
#define CMU_BASE_ADDR                   0x400C8000uL
#define CMU_HFCORECLKEN0                (*(volatile U32*) (CMU_BASE_ADDR + 0x040))     // High Frequency Core Clock Enable Register 0
#define CMU_HFPERCLKEN0                 (*(volatile U32*) (CMU_BASE_ADDR + 0x044))     // High Frequency Peripheral Clock Enable Register 0

/*********************************************************************
*
*       DMA
*/
#define DMA_BASE_ADDR                   0x400C2000uL
#define DMA_CONFIG                      (*(volatile U32*) (DMA_BASE_ADDR + 0x0004))    // DMA Configuration Register
#define DMA_CTRLBASE                    (*(volatile U32*) (DMA_BASE_ADDR + 0x0008))    // Channel Control Data Base Pointer Register
#define DMA_CHREQMASKC                  (*(volatile U32*) (DMA_BASE_ADDR + 0x0024))    // Channel Request Mask Clear Register
#define DMA_CHENS                       (*(volatile U32*) (DMA_BASE_ADDR + 0x0028))    // Channel Enable Set Register
#define DMA_CHALTC                      (*(volatile U32*) (DMA_BASE_ADDR + 0x0034))    // Channel Alternate Clear Register
#define DMA_ERRORC                      (*(volatile U32*) (DMA_BASE_ADDR + 0x004C))    // Bus Error Clear Register
#define DMA_CH_CTRL_TX                  (*(volatile U32*) (DMA_BASE_ADDR + 0x1100 + FS_NOR_HW_SPI_DMA_CHANNEL_TX * 4))    // Channel Control Register (send to NOR flash)
#define DMA_CH_CTRL_RX                  (*(volatile U32*) (DMA_BASE_ADDR + 0x1100 + FS_NOR_HW_SPI_DMA_CHANNEL_RX * 4))    // Channel Control Register (receive from NOR flash)

/*********************************************************************
*
*       Pin assignments of NOR flash signals
*/
#define NOR_CS_PIN                      4   // Chip select
#define NOR_CLK_PIN                     5   // Clock
#define NOR_MISO_PIN                    6   // Data from NOR flash
#define NOR_MOSI_PIN                    7   // Data to NOR flash

/*********************************************************************
*
*       GPIO modes
*/
#define MODE_PUSHPULL                   4

/*********************************************************************
*
*       USART control
*/
#define CTRL_SYNC                       0
#define CTRL_CLKPOL                     8
#define CTRL_CLKPHA                     9
#define CTRL_MSBF                       10

/*********************************************************************
*
*       USART commands
*/
#define CMD_RXEN                        0
#define CMD_TXEN                        2
#define CMD_MASTEREN                    4
#define CMD_CLEARTX                     10
#define CMD_CLEARRX                     11

/*********************************************************************
*
*       USART status
*/
#define STATUS_TXBL                     6
#define STATUS_RXDATAV                  7
#define STATUS_TXC                      5

/*********************************************************************
*
*       USART signal routing
*/
#define ROUTE_RXPEN                     0
#define ROUTE_TXPEN                     1
#define ROUTE_CLKPEN                    3
#define ROUTE_LOCATION                  8

/*********************************************************************
*
*       Peripheral clock control
*/
#define HFPERCLKEN0_USART0              0
#define HFPERCLKEN0_GPIO                13
#define HFCORECLKEN0_DMA                0

/*********************************************************************
*
*       DMA global configuration
*/
#define CONFIG_EN                       0

/*********************************************************************
*
*       DMA descriptor configuration
*/
#define CONFIG_DST_INC_NONE             (3uL << 30)
#define CONFIG_SRC_INC_NONE             (3uL << 26)
#define CONFIG_N_MINUS_1_SHIFT          4
#define CONFIG_N_MINUS_1_MASK           0x3FF
#define CONFIG_CYCLE_CTRL_BASIC         1uL
#define CONFIG_CYCLE_CTRL_MASK          0x7uL

/*********************************************************************
*
*       DMA request configuration
*/
#define CH_CTRL_SOURCESEL_USART0        (0xCuL << 16)
#define CH_CTRL_SIGSEL_USART0RXDATAV    0uL
#define CH_CTRL_SIGSEL_USART0TXBL       1uL

/*********************************************************************
*
*       Number of DMA descriptors
*/
#if (FS_NOR_HW_SPI_DMA_CHANNEL_TX >= FS_NOR_HW_SPI_DMA_CHANNEL_RX)
  #define NUM_DMA_DESC                  (FS_NOR_HW_SPI_DMA_CHANNEL_TX + 1)
#else
  #define NUM_DMA_DESC                  (FS_NOR_HW_SPI_DMA_CHANNEL_RX + 1)
#endif

/*********************************************************************
*
*       Types, local
*
**********************************************************************
*/
typedef struct {
  volatile U32 SrcEndAddr;    // Last address DMA reads from
  volatile U32 DestEndAddr;   // Last address DMA writes to
  volatile U32 Config;        // Transfer configuration
  volatile U32 Padding;
} DMA_DESC;

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

#if FS_NOR_HW_SPI_USE_DMA
  static U32 _Dummy;
  //
  // The DMA descriptor requires 256 byte alignment
  //
  #if defined (__ICCARM__)
    #pragma data_alignment=256
    static DMA_DESC _aDMADesc[NUM_DMA_DESC];  // Only the primary descriptors are used
  #elif defined (__SES_ARM)
    static DMA_DESC _aDMADesc[NUM_DMA_DESC] __attribute__ ((aligned (256)));
  #else
    #error Compiler not supported!
  #endif
#endif

/*********************************************************************
*
*       Local functions
*
**********************************************************************
*/

#if FS_NOR_HW_SPI_USE_DMA

/*********************************************************************
*
*       _StartDMAWrite
*
*   Function description
*     Sends data to NOR flash via SPI using DMA.
*
*   Parameters
*     pData     [IN]  Data to send
*               [OUT] ---
*     NumBytes  Number of bytes to send
*/
static void _StartDMAWrite(const U8 * pData, int NumBytes) {
  DMA_DESC * pDMADescTx;
  DMA_DESC * pDMADescRx;
  int        NumItemsMinus1;

  NumItemsMinus1 = NumBytes - 1;
  //
  // Configure the DMA channel that sends data
  //
  pDMADescTx              = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_TX];
  pDMADescTx->SrcEndAddr  = (U32)(pData + NumItemsMinus1);
  pDMADescTx->DestEndAddr = (U32)&USART0_TXDATA;
  pDMADescTx->Config      = CONFIG_DST_INC_NONE
                          | (NumItemsMinus1 << CONFIG_N_MINUS_1_SHIFT)
                          | CONFIG_CYCLE_CTRL_BASIC
                          ;
  //
  // Select the buffer low signal of USART0 as the transfer request.
  //
  DMA_CH_CTRL_TX          = CH_CTRL_SOURCESEL_USART0
                          | CH_CTRL_SIGSEL_USART0TXBL
                          ;
  //
  // Configure the DMA channel that receives data. All data is discarded.
  //
  pDMADescRx              = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_RX];
  pDMADescRx->SrcEndAddr  = (U32)&USART0_RXDATA;
  pDMADescRx->DestEndAddr = (U32)&_Dummy;
  pDMADescRx->Config      = CONFIG_DST_INC_NONE
                          | CONFIG_SRC_INC_NONE
                          | (NumItemsMinus1 << CONFIG_N_MINUS_1_SHIFT)
                          | CONFIG_CYCLE_CTRL_BASIC
                          ;
  //
  // Select the data available signal of USART0 as the transfer request.
  //
  DMA_CH_CTRL_RX          = CH_CTRL_SOURCESEL_USART0
                          | CH_CTRL_SIGSEL_USART0RXDATAV
                          ;
  //
  // Start the transfer on both DMA channels
  //
  DMA_CHREQMASKC  = 0
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Enable the requests from peripheral
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                  ;
  DMA_CHALTC      = 0
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Use the primary channel (disable the alternate one)
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                  ;
  DMA_CHENS      |= 0
                 |  (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Enable the channels
                 |  (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                 ;
}

/*********************************************************************
*
*       _StartDMARead
*
*   Function description
*     Receives data from NOR flash via SPI using DMA.
*
*   Parameters
*     pData     [IN]  ---
*               [OUT] Data received from NOR flash
*     NumBytes  Number of bytes to receive
*/
static void _StartDMARead(U8 * pData, int NumBytes) {
  DMA_DESC * pDMADescRx;
  DMA_DESC * pDMADescTx;
  int        NumItemsMinus1;

  NumItemsMinus1 = NumBytes - 1;
  _Dummy         = 0xFFFFFFFF;
  //
  // Configure the DMA channel that receives data
  //
  pDMADescRx              = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_RX];
  pDMADescRx->SrcEndAddr  = (U32)&USART0_RXDATA;
  pDMADescRx->DestEndAddr = (U32)(pData + NumItemsMinus1);
  pDMADescRx->Config      = CONFIG_SRC_INC_NONE
                          | (NumItemsMinus1 << CONFIG_N_MINUS_1_SHIFT)
                          | CONFIG_CYCLE_CTRL_BASIC
                          ;
  DMA_CH_CTRL_RX          = CH_CTRL_SOURCESEL_USART0      // Select the data available signal of USART0 as the transfer request
                          | CH_CTRL_SIGSEL_USART0RXDATAV
                          ;
  //
  // Configure the DMA channel that sends data. We need to send something in order to receive.
  //
  pDMADescTx              = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_TX];
  pDMADescTx->SrcEndAddr  = (U32)&_Dummy;
  pDMADescTx->DestEndAddr = (U32)&USART0_TXDATA;
  pDMADescTx->Config      = CONFIG_DST_INC_NONE
                          | CONFIG_SRC_INC_NONE
                          | (NumItemsMinus1 << CONFIG_N_MINUS_1_SHIFT)
                          | CONFIG_CYCLE_CTRL_BASIC
                          ;
  DMA_CH_CTRL_TX          = CH_CTRL_SOURCESEL_USART0      // Select the buffer empty signal of USART0 as the transfer request
                          | CH_CTRL_SIGSEL_USART0TXBL
                          ;
  //
  // Start the transfer on both DMA channels
  //
  DMA_CHREQMASKC  = (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Enable the requests from peripheral
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                  ;
  DMA_CHALTC      = (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Use the primary channel (disable the alternate one)
                  | (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                  ;
  DMA_CHENS      |= (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_TX)   // Enable the channels
                 |  (1uL << FS_NOR_HW_SPI_DMA_CHANNEL_RX)
                 ;
}

/*********************************************************************
*
*       _WaitForDMATransferEnd
*
*   Function description
*     Waits for the current DMA transfer to end.
*/
static void _WaitForDMATransferEnd(void) {
  U32        Status;
  DMA_DESC * pDMADesc;

  for (;;) {
    //
    // Check for error first
    //
    Status = DMA_ERRORC;
    if (Status != 0u) {
      DMA_ERRORC = 1;   // Clear the error condition and exit
      break;
    }
    //
    // Check for the end of data transfer
    //
    pDMADesc  = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_RX];
    Status    = pDMADesc->Config;
    pDMADesc  = &_aDMADesc[FS_NOR_HW_SPI_DMA_CHANNEL_TX];
    Status   |= pDMADesc->Config;
    if ((Status & CONFIG_CYCLE_CTRL_MASK) == 0) {   // DMA controller sets the cycle control to 0 when all items have been transferred
      break;
    }
  }
}

#endif  // FS_NOR_HW_SPI_USE_DMA

/*********************************************************************
*
*       _Init
*/
static void _Init(void) {
  //
  // Enable clocks
  //
  CMU_HFPERCLKEN0 |= (1uL << HFPERCLKEN0_USART0)
                  |  (1uL << HFPERCLKEN0_GPIO)
                  ;
  //
  // Configure the I/O pins used by USART
  //
  GPIOE_MODEL &= ~((0xFuL << (NOR_CS_PIN   << 2)) |
                   (0xFuL << (NOR_CLK_PIN  << 2)) |
                   (0xFuL << (NOR_MISO_PIN << 2)) |
                   (0xFuL << (NOR_MOSI_PIN << 2)));
  GPIOE_MODEL |= (MODE_PUSHPULL << (NOR_CS_PIN   << 2))
              |  (MODE_PUSHPULL << (NOR_CLK_PIN  << 2))
              |  (MODE_PUSHPULL << (NOR_MISO_PIN << 2))
              |  (MODE_PUSHPULL << (NOR_MOSI_PIN << 2))
              ;
  GPIOE_DOUTCLR = (1uL << NOR_CS_PIN)
                | (1uL << NOR_CLK_PIN)
                | (1uL << NOR_MISO_PIN)
                | (1uL << NOR_MOSI_PIN)
                ;
  //
  // Initialize DMA
  //
#if FS_NOR_HW_SPI_USE_DMA
  CMU_HFCORECLKEN0 |= (1uL << HFCORECLKEN0_DMA);
  DMA_CTRLBASE      = (U32)&_aDMADesc;
  DMA_CONFIG       |= (1uL << CONFIG_EN);         // Enable DMA
#endif
  //
  // Configure USART as SPI
  //
  USART0_CTRL  = (1uL << CTRL_SYNC)
               | (1uL << CTRL_MSBF)
               | (1uL << CTRL_CLKPOL)
               | (1uL << CTRL_CLKPHA)
               ;
  USART0_CMD   = (1uL << CMD_CLEARRX)
               | (1uL << CMD_CLEARTX)
               ;
  USART0_IEN   = 0;   // Polling mode, no interrupts
  USART0_ROUTE = (1uL << ROUTE_TXPEN)
               | (1uL << ROUTE_RXPEN)
               | (1uL << ROUTE_CLKPEN)
               | (1uL << ROUTE_LOCATION)
               ;
  USART0_CMD   = (1uL << CMD_TXEN)
               | (1uL << CMD_RXEN)
               ;
  USART0_IFC   = 0x00001FF9uL;
  USART0_CMD   = (1uL << CMD_MASTEREN);
}

/*********************************************************************
*
*       _SetMaxSpeed
*
*  Function Description
*    Sets the SPI interface to a maximum frequency.
*
*  Return value
*    max. frequency   The maximum frequency set in kHz
*    ==0              The frequency could not be set
*/
static U16 _SetMaxSpeed(void) {
  U32 ClkDiv;
  int MaxFreq;

  ClkDiv = 0;
  //
  // Clock frequency is computed by the following formula:
  //
  // SPIFreq = FS_NOR_HW_SPI_PER_CLK_HZ / (2 * (1 + ClkDiv / 256))
  //
  ClkDiv  = FS_NOR_HW_SPI_PER_CLK_HZ / FS_NOR_HW_SPI_NOR_CLK_HZ * 128;
  if (ClkDiv > 256) {
    ClkDiv -= 256;
  } else {
    ClkDiv = 0;
  }
  USART0_CLKDIV = ClkDiv;
  //
  // We must return the "real" SPI clock frequency the HW
  // generates using the computed clock divisor. The value is in kHz.
  //
  MaxFreq = FS_NOR_HW_SPI_PER_CLK_HZ / (2 * (1 + (ClkDiv + 256 / 2) / 256)) / 1000;
  return MaxFreq;       // We are not faster than this
}

/*********************************************************************
*
*       Local functions
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
  int MaxFreq;

  FS_USE_PARA(Unit);
  _Init();
  MaxFreq = (int)_SetMaxSpeed();
  return MaxFreq;
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
  GPIOE_DOUTCLR = 1uL << NOR_CS_PIN;    // Active low
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
  GPIOE_DOUTSET = 1uL << NOR_CS_PIN;   // Active low
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
#if FS_NOR_HW_SPI_USE_DMA
  int NumBytesAtOnce;

  do {
    NumBytesAtOnce = SEGGER_MIN(NumBytes, CONFIG_N_MINUS_1_MASK + 1);
    _StartDMARead(pData, NumBytesAtOnce);
    _WaitForDMATransferEnd();
    NumBytes -= NumBytesAtOnce;
    pData    += NumBytesAtOnce;
  } while (NumBytes);
#else
  volatile U8 Data;

  Data = USART0_RXDATA;                                   // Do a dummy read to clear RXDATAV flag
  FS_USE_PARA(Data);
  while (!(USART0_STATUS & (1uL << STATUS_TXBL))) {       // Wait for the previous data transfer to end
    ;
  }
  USART0_CMD = 0
             | (1uL << CMD_CLEARRX)
             | (1uL << CMD_CLEARTX)
             ;
  do {
    USART0_TXDATA = 0xFF;                                 // Start the transfer of one byte
    while (!(USART0_STATUS & (1uL << STATUS_RXDATAV))) {  // Wait for the byte to be received
      ;
    }
    *pData++ = (U8)USART0_RXDATA;
  } while (--NumBytes);
#endif
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
#if FS_NOR_HW_SPI_USE_DMA
  int NumBytesAtOnce;

  do {
    NumBytesAtOnce = SEGGER_MIN(NumBytes, CONFIG_N_MINUS_1_MASK + 1);
    _StartDMAWrite(pData, NumBytesAtOnce);
    _WaitForDMATransferEnd();
    NumBytes -= NumBytesAtOnce;
    pData    += NumBytesAtOnce;
  } while (NumBytes);
#else
  do {
    while (!(USART0_STATUS & (1uL << STATUS_TXBL))) {     // Wait for the previous data transfer to end
      ;
    }
    USART0_TXDATA = (U32)*pData++;                        // Transfer one byte at a time
  } while (--NumBytes);
  while (!(USART0_STATUS & (1uL << STATUS_TXC))) {        // Wait for the last bit to be sent
    ;
  }
#endif
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_EFM32GG_EnergyMicro_EFR4D_STK6500 = {
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
