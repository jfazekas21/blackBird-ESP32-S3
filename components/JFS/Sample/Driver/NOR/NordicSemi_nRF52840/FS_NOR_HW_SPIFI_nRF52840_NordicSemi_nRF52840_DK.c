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

File    : FS_NOR_HW_SPIFI_nRF52840_NordicSemi_nRF52840_DK.c
Purpose : Template low-level flash driver for quad SPI.
Literature:
  [1] nRF52840 Product Specification
    (\\FILESERVER\Techinfo\Company\NordicSemiconductor\MCU\nRF52\nRF52840_PS_v1.0.pdf)
  [2] nRF52840 Rev 1 Errata
    (\\FILESERVER\Techinfo\Company\NordicSemiconductor\MCU\nRF52\nRF52840_Rev_1_Errata_v1.1.pdf)
  [3] nRF52840 Development Kit v1.0.0 User Guide
    (\\FILESERVER\Techinfo\Company\NordicSemiconductor\MCU\nRF52\EvalBoard\nRF52840\nRF52840_DK_User_Guide_v1.0.pdf)
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
#define PER_CLK_HZ          32000000          // Clock frequency of SPI peripheral.
#define NOR_CLK_HZ          (PER_CLK_HZ / 2)  // Frequency of the clock supplied to NOR flash device.

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
#define QSPI_BASE_ADDR                0x40029000uL
#define QSPI_TASKS_ACTIVATE           (*(volatile U32*)(QSPI_BASE_ADDR + 0x000))
#define QSPI_TASKS_READ_START         (*(volatile U32*)(QSPI_BASE_ADDR + 0x004))
#define QSPI_TASKS_WRITE_START        (*(volatile U32*)(QSPI_BASE_ADDR + 0x008))
#define QSPI_TASKS_ERASE_START        (*(volatile U32*)(QSPI_BASE_ADDR + 0x00C))
#define QSPI_TASKS_DEACTIVATE         (*(volatile U32*)(QSPI_BASE_ADDR + 0x010))
#define QSPI_EVENTS_READY             (*(volatile U32*)(QSPI_BASE_ADDR + 0x100))
#define QSPI_INTEN                    (*(volatile U32*)(QSPI_BASE_ADDR + 0x300))
#define QSPI_INTENSET                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x304))
#define QSPI_INTENCLR                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x308))
#define QSPI_ENABLE                   (*(volatile U32*)(QSPI_BASE_ADDR + 0x500))
#define QSPI_READ_SRC                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x504))
#define QSPI_READ_DST                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x508))
#define QSPI_READ_CNT                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x50C))
#define QSPI_WRITE_DST                (*(volatile U32*)(QSPI_BASE_ADDR + 0x510))
#define QSPI_WRITE_SRC                (*(volatile U32*)(QSPI_BASE_ADDR + 0x514))
#define QSPI_WRITE_CNT                (*(volatile U32*)(QSPI_BASE_ADDR + 0x518))
#define QSPI_ERASE_PTR                (*(volatile U32*)(QSPI_BASE_ADDR + 0x51C))
#define QSPI_ERASE_LEN                (*(volatile U32*)(QSPI_BASE_ADDR + 0x520))
#define QSPI_PSEL_SCK                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x524))
#define QSPI_PSEL_CSN                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x528))
#define QSPI_PSEL_IO0                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x530))
#define QSPI_PSEL_IO1                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x534))
#define QSPI_PSEL_IO2                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x538))
#define QSPI_PSEL_IO3                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x53C))
#define QSPI_XIPOFFSET                (*(volatile U32*)(QSPI_BASE_ADDR + 0x540))
#define QSPI_IFCONFIG0                (*(volatile U32*)(QSPI_BASE_ADDR + 0x544))
#define QSPI_IFCONFIG1                (*(volatile U32*)(QSPI_BASE_ADDR + 0x600))
#define QSPI_STATUS                   (*(volatile U32*)(QSPI_BASE_ADDR + 0x604))
#define QSPI_DPMDUR                   (*(volatile U32*)(QSPI_BASE_ADDR + 0x614))
#define QSPI_ADDRCONF                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x624))
#define QSPI_CINSTRCONF               (*(volatile U32*)(QSPI_BASE_ADDR + 0x634))
#define QSPI_CINSTRDAT0               (*(volatile U32*)(QSPI_BASE_ADDR + 0x638))
#define QSPI_CINSTRDAT1               (*(volatile U32*)(QSPI_BASE_ADDR + 0x63C))
#define QSPI_IFTIMING                 (*(volatile U32*)(QSPI_BASE_ADDR + 0x640))

/*********************************************************************
*
*       P0 registers
*/
#define P0_BASE_ADDR                  0x50000000uL
#define P0_PIN_CNF17                  (*(volatile U32*)(P0_BASE_ADDR + 0x744))
#define P0_PIN_CNF19                  (*(volatile U32*)(P0_BASE_ADDR + 0x74C))
#define P0_PIN_CNF20                  (*(volatile U32*)(P0_BASE_ADDR + 0x750))
#define P0_PIN_CNF21                  (*(volatile U32*)(P0_BASE_ADDR + 0x754))
#define P0_PIN_CNF22                  (*(volatile U32*)(P0_BASE_ADDR + 0x758))
#define P0_PIN_CNF23                  (*(volatile U32*)(P0_BASE_ADDR + 0x75C))

/*********************************************************************
*
*       IFCONFIG0 register
*/
#define IFCONFIG0_READOC_BIT          0
#define IFCONFIG0_READOC_FASTREAD     0
#define IFCONFIG0_READOC_READ2O       1
#define IFCONFIG0_READOC_READ2IO      2
#define IFCONFIG0_READOC_READ4O       3
#define IFCONFIG0_READOC_READ4IO      4
#define IFCONFIG0_READOC_MASK         0x7uL
#define IFCONFIG0_WRITEOC_BIT         3
#define IFCONFIG0_WRITEOC_PP          0
#define IFCONFIG0_WRITEOC_PP2O        1
#define IFCONFIG0_WRITEOC_PP4O        2
#define IFCONFIG0_WRITEOC_PP4IO       3
#define IFCONFIG0_WRITEOC_MASK        0x7uL
#define IFCONFIG0_ADDRMODE_BIT        6

/*********************************************************************
*
*       IFCONFIG1 register
*/
#define IFCONFIG1_SCKDELAY_BIT        0
#define IFCONFIG1_SCKFREQ_BIT         28
#define IFCONFIG1_SCKFREQ_MAX         0xFuL

/*********************************************************************
*
*       CINSTRCONF register
*/
#define CINSTRCONF_OPCODE_BIT         0
#define CINSTRCONF_LENGTH_BIT         8
#define CINSTRCONF_LIO2_BIT           12
#define CINSTRCONF_LIO3_BIT           13
#define CINSTRCONF_LFEN_BIT           16
#define CINSTRCONF_LFSTOP_BIT         17

/*********************************************************************
*
*       Pin configuration register (QSPI)
*/
#define PSEL_PIN_BIT                  0
#define PSEL_PORT_BIT                 5

/*********************************************************************
*
*       Pin configuration register (GPIO)
*/
#define PIN_CNF_DRIVE_BIT             8
#define PIN_CNF_DRIVE_MASK            0x7uL
#define PIN_CNF_DRIVE_H0H1            3uL

/*********************************************************************
*
*       Pin configuration
*/
#define NOR_SCK_PIN                   19
#define NOR_CS_PIN                    17
#define NOR_IO0_PIN                   20
#define NOR_IO1_PIN                   21
#define NOR_IO2_PIN                   22
#define NOR_IO3_PIN                   23
#define NOR_SCK_PORT                  0
#define NOR_CS_PORT                   0
#define NOR_IO0_PORT                  0
#define NOR_IO1_PORT                  0
#define NOR_IO2_PORT                  0
#define NOR_IO3_PORT                  0

/*********************************************************************
*
*       Misc. defines
*/
#define ENABLE_ENABLE_BIT             0
#define WAIT_TIMEOUT_CYCLES           10000000uL   // About 1s at 32MHz
#define QSPI_BUFFER_SIZE              8
#define MAX_READ_CNT                  0x3FFFFuL
#define CMD_READ_FAST                 0x0B
#define MAX_WRITE_CNT                 0x3FFFFuL

/*********************************************************************
*
*      Static data
*
**********************************************************************
*/
static U8 _DMAReadMode;
static U8 _DMAWriteMode;
static U8 _Is4ByteAddr;

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

  Div        = 0;
  Freq_Hz    = PER_CLK_HZ;
  MaxFreq_Hz = *pFreq_Hz;
  for (;;) {
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    Freq_Hz >>= 1;
    ++Div;
    if (Div == IFCONFIG1_SCKFREQ_MAX) {
      break;
    }
  }
  *pFreq_Hz = Freq_Hz;
  return Div;
}

/*********************************************************************
*
*       _WaitForReady
*/
static int _WaitForReady(void) {
  U32 TimeOut;
  int r;

  r       = 0;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if (QSPI_EVENTS_READY != 0) {
      break;                            // OK.
    }
    if (--TimeOut == 0) {
      r = 1;                            // Error, timeout exceeded.
      break;
    }
  }
  return r;
}

/*********************************************************************
*
*       _StoreToBuffer
*/
static void _StoreToBuffer(const U8 * pData, unsigned NumBytes) {
  U32 v;

  v = 0;
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytes <= 8);
  switch (NumBytes) {
  case 8:
    v |= ((U32)*(pData + 7)) << 24;
    // fall-through
  case 7:
    v |= ((U32)*(pData + 6)) << 16;
    // fall-through
  case 6:
    v |= ((U32)*(pData + 5)) << 8;
    // fall-through
  case 5:
    v |= (U32)*(pData + 4);
    QSPI_CINSTRDAT1 = v;
    v = 0;
    // fall-through
  case 4:
    v  |= ((U32)*(pData + 3)) << 24;
    // fall-through
  case 3:
    v |= ((U32)*(pData + 2)) << 16;
    // fall-through
  case 2:
    v |= ((U32)*(pData + 1)) << 8;
    // fall-through
  case 1:
    v |= (U32)*pData;
    // fall-through
    QSPI_CINSTRDAT0 = v;
  default:
    break;
  }
}

/*********************************************************************
*
*       _LoadFromBuffer
*/
static void _LoadFromBuffer(U8 * pData, unsigned NumBytes) {
  U32 v0;
  U32 v1;

  v0 = QSPI_CINSTRDAT0;
  v1 = QSPI_CINSTRDAT1;
  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytes <= 8);
  switch (NumBytes) {
  case 8:
    *(pData + 7) = (U8)(v1 >> 24);
    // fall-through
  case 7:
    *(pData + 6) = (U8)(v1 >> 16);
    // fall-through
  case 6:
    *(pData + 5) = (U8)(v1 >> 8);
    // fall-through
  case 5:
    *(pData + 4) = (U32)v1;
    // fall-through
  case 4:
    *(pData + 3) = (U8)(v0 >> 24);
    // fall-through
  case 3:
    *(pData + 2) = (U8)(v0 >> 16);
    // fall-through
  case 2:
    *(pData + 1) = (U8)(v0 >> 8);
    // fall-through
  case 1:
    *pData = (U8)v0;
    // fall-through
  default:
    break;
  }
}

/*********************************************************************
*
*       _Init
*/
static int _Init(void) {
  U32 v;
  U32 Div;
  U32 Freq_Hz;
  int r;

  //
  // Configure port pins.
  //
  QSPI_PSEL_SCK = 0
                | (NOR_SCK_PIN  << PSEL_PIN_BIT)
                | (NOR_SCK_PORT << PSEL_PORT_BIT)
                ;
  QSPI_PSEL_CSN = 0
                | (NOR_CS_PIN   << PSEL_PIN_BIT)
                | (NOR_CS_PORT  << PSEL_PORT_BIT)
                ;
  QSPI_PSEL_IO0 = 0
                | (NOR_IO0_PIN  << PSEL_PIN_BIT)
                | (NOR_IO0_PORT << PSEL_PORT_BIT)
                ;
  QSPI_PSEL_IO1 = 0
                | (NOR_IO1_PIN  << PSEL_PIN_BIT)
                | (NOR_IO1_PORT << PSEL_PORT_BIT)
                ;
  QSPI_PSEL_IO2 = 0
                | (NOR_IO2_PIN  << PSEL_PIN_BIT)
                | (NOR_IO2_PORT << PSEL_PORT_BIT)
                ;
  QSPI_PSEL_IO3 = 0
                | (NOR_IO3_PIN  << PSEL_PIN_BIT)
                | (NOR_IO3_PORT << PSEL_PORT_BIT)
                ;
  //
  // According to [1] the drive strength of port pins has
  // to be set to "high drive" for correct operation.
  //
  P0_PIN_CNF17 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF17 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF19 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF19 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF20 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF20 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF21 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF21 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF22 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF22 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF23 &= ~(PIN_CNF_DRIVE_MASK << PIN_CNF_DRIVE_BIT);
  P0_PIN_CNF23 |=  (PIN_CNF_DRIVE_H0H1 << PIN_CNF_DRIVE_BIT);
  //
  // Calculate the clock divisor.
  //
  Freq_Hz = NOR_CLK_HZ;
  Div = _CalcClockDivider(&Freq_Hz);
  //
  // Configure QSPI peripheral.
  //
  QSPI_XIPOFFSET      = 0;
  QSPI_IFCONFIG0      = 0
                      | (_DMAReadMode  << IFCONFIG0_READOC_BIT)
                      | (_DMAWriteMode << IFCONFIG0_WRITEOC_BIT)
                      | (_Is4ByteAddr  << IFCONFIG0_ADDRMODE_BIT)
                      ;
  v                   = QSPI_IFCONFIG1;
  v                   &= 0x00FFFF00;      // Preserve the value of reserved bits.
  v                   = 0
                      | (1uL << IFCONFIG1_SCKDELAY_BIT)
                      | (Div << IFCONFIG1_SCKFREQ_BIT)
                      ;
  QSPI_IFCONFIG1      = v;
  QSPI_INTENCLR       = 0xFFFFFFFFuL;     // Clear all interrupts.
  QSPI_ENABLE         = 1uL << ENABLE_ENABLE_BIT;
  QSPI_EVENTS_READY   = 0;                // Clear event.
  QSPI_TASKS_ACTIVATE = 1uL;
  r = _WaitForReady();
  if (r != 0) {
    Freq_Hz = 0;                          // Error, could not activate the QSPI peripheral.
  }
  return Freq_Hz;
}

/*********************************************************************
*
*       _DeInit
*/
static void _DeInit(void) {
  QSPI_TASKS_DEACTIVATE         = 1uL;
  //
  // Workaround for nRF52840 anomaly 122: Current consumption is too high.
  //
  *(volatile U32 *)0x40029054ul = 1uL;
  QSPI_ENABLE                   = 0;
}

/*********************************************************************
*
*       _SendCmd
*
*  Notes
*    (1) We have to set the IO2 and IO3 data lines at HIGH otherwise
*        the data transfer will not work.
*/
static int _SendCmd(U8 Cmd) {
  int r;

  QSPI_EVENTS_READY = 0;          // Clear event.
  QSPI_CINSTRCONF   = 0
                    | ((U32)Cmd << CINSTRCONF_OPCODE_BIT)
                    | (1uL      << CINSTRCONF_LENGTH_BIT)
                    | (1uL      << CINSTRCONF_LIO2_BIT)   // Note 1
                    | (1uL      << CINSTRCONF_LIO3_BIT)   // Note 1
                    ;
  r = _WaitForReady();
  return r;
}

/*********************************************************************
*
*       _SendCmdAndData
*
*  Notes
*    (1) We have to set the IO2 and IO3 data lines at HIGH otherwise
*        the data transfer will not work.
*/
static int _SendCmdAndData(U8 Cmd, const U8 * pData, unsigned NumBytes, U8 IsMoreData) {
  int r;

  FS_DEBUG_ASSERT(FS_MTYPE_DRIVER, NumBytes <= 8);
  _StoreToBuffer(pData, NumBytes);
  ++NumBytes;                       // ++ for the command byte.
  QSPI_EVENTS_READY = 0;          // Clear event.
  QSPI_CINSTRCONF   = 0
                    | ((U32)Cmd   << CINSTRCONF_OPCODE_BIT)
                    | (NumBytes   << CINSTRCONF_LENGTH_BIT)
                    | (IsMoreData << CINSTRCONF_LFEN_BIT)
                    | (1uL        << CINSTRCONF_LIO2_BIT)   // Note 1
                    | (1uL        << CINSTRCONF_LIO3_BIT)   // Note 1
                    ;
  r = _WaitForReady();
  return r;
}

/*********************************************************************
*
*       _SendData
*
*  Notes
*    (1) We have to set the IO2 and IO3 data lines at HIGH otherwise
*        the data transfer will not work.
*/
static int _SendData(const U8 * pData, unsigned NumBytes) {
  U32 NumBytesAtOnce;
  int r;
  U8  IsEndOfData;

  r = 0;
  if (NumBytes != 0u) {
    IsEndOfData = 0;
    do {
      NumBytesAtOnce = SEGGER_MIN(NumBytes, QSPI_BUFFER_SIZE);
      _StoreToBuffer(pData, NumBytesAtOnce);
      if ((NumBytes - NumBytesAtOnce) == 0) {
        IsEndOfData = 1;
      }
      QSPI_EVENTS_READY = 0;          // Clear event.
      QSPI_CINSTRCONF   = 0
                        | ((NumBytesAtOnce + 1) << CINSTRCONF_LENGTH_BIT) // +1 for the command byte that is actually not transferred.
                        | (1uL                  << CINSTRCONF_LFEN_BIT)
                        | (IsEndOfData          << CINSTRCONF_LFSTOP_BIT)
                        | (1uL                  << CINSTRCONF_LIO2_BIT)   // Note 1
                        | (1uL                  << CINSTRCONF_LIO3_BIT)   // Note 1
                        ;
      r = _WaitForReady();
      if (r != 0) {
        break;
      }
      NumBytes -= NumBytesAtOnce;
      pData    += NumBytesAtOnce;
    } while (NumBytes != 0u);
  }
  return r;
}

/*********************************************************************
*
*      _ReceiveData
*
*  Notes
*    (1) We have to set the IO2 and IO3 data lines at HIGH otherwise
*        the data transfer will not work.
*/
static int _ReceiveData(U8 * pData, unsigned NumBytes) {
  U32 NumBytesAtOnce;
  int r;
  U8  IsEndOfData;

  r = 0;
  if (NumBytes != 0u) {
    IsEndOfData = 0;
    do {
      NumBytesAtOnce = SEGGER_MIN(NumBytes, QSPI_BUFFER_SIZE);
      if ((NumBytes - NumBytesAtOnce) == 0) {
        IsEndOfData = 1;
      }
      QSPI_CINSTRDAT0   = 0;
      QSPI_CINSTRDAT1   = 0;
      QSPI_EVENTS_READY = 0;          // Clear event.
      QSPI_CINSTRCONF   = 0
                        | ((NumBytesAtOnce + 1) << CINSTRCONF_LENGTH_BIT) // +1 for the command byte that is actually not transferred.
                        | (1uL                  << CINSTRCONF_LFEN_BIT)
                        | (IsEndOfData          << CINSTRCONF_LFSTOP_BIT)
                        | (1uL                  << CINSTRCONF_LIO2_BIT)   // Note 1
                        | (1uL                  << CINSTRCONF_LIO3_BIT)   // Note 1
                        ;
      r = _WaitForReady();
      if (r != 0) {
        break;
      }
      _LoadFromBuffer(pData, NumBytesAtOnce);
      NumBytes -= NumBytesAtOnce;
      pData    += NumBytesAtOnce;
    } while (NumBytes != 0u);
  }
  return r;
}

/*********************************************************************
*
*       _ReadDataViaCPU
*/
static int _ReadDataViaCPU(U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  U8  IsMoreData;
  int r;

  //
  // The data cannot be read from the memory array of NOR flash
  // using dual or quad SPI mode via CPU. This is a limitation
  // of the QSPI peripheral. Therefore we have to modify the command
  // parameters here to read the data using single SPI mode.
  // We know that the SPIFI physical layer uses dual and quad SPI mode
  // only for read operations from the memory array of the NOR flash
  // so we can simply check for this condition and set the command
  // code to READ FAST (0x0B) and set the number of dummy cycles to 1 byte.
  //
  if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) > 1)
      || (FS_BUSWIDTH_GET_DATA(BusWidth) > 1)) {
    Cmd = CMD_READ_FAST;
    if ((NumBytesPara - NumBytesAddr) > 1) {
      NumBytesPara = NumBytesAddr + 1;
    }
  }
  IsMoreData = 0;
  if (NumBytesData != 0u) {
    IsMoreData = 1;
  }
  r = _SendCmdAndData(Cmd, pPara, NumBytesPara, IsMoreData);
  if (r == 0) {
    if (IsMoreData != 0u) {
      r = _ReceiveData(pData, NumBytesData);
    }
  }
  return r;
}

/*********************************************************************
*
*      _GetDMAReadMode
*/
static int _GetDMAReadMode(U8 Cmd, U16 BusWidth) {
  int ReadMode;

  ReadMode = -1;                        // Set to indicate that read via DMA is not supported.
  //
  // Check that the command and the bus configuration matches
  // one of the read codes supported by the QSPI peripheral.
  //
  switch (Cmd) {
  case 0x0B:          // Address and data are single SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 1)) {
      ReadMode = IFCONFIG0_READOC_FASTREAD;
    }
    break;
  case 0x3B:          // Address is single SPI, data is dual SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 2)) {
      ReadMode = IFCONFIG0_READOC_READ2O;
    }
    break;
  case 0xBB:          // Address and data are dual SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 2)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 2)) {
      ReadMode = IFCONFIG0_READOC_READ2IO;
    }
    break;
  case 0x6B:          // Address is single SPI, data is quad SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 4)) {
      ReadMode = IFCONFIG0_READOC_READ4O;
    }
    break;
  case 0xEB:          // Address and data are quad SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 4)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 4)) {
      ReadMode = IFCONFIG0_READOC_READ4IO;
    }
    break;
  default:
    break;
  }
  return ReadMode;
}

/*********************************************************************
*
*      _IsDMATransferAllowed
*
*  Function description
*    Checks if the data can be read via DMA.
*
*  Return value
*    ==1      The data can be read via DMA.
*    ==0      The data has to be read via CPU in single SPI mode.
*
*  Additional information
*    The data can be read via DMA if:
*    - the destination address is 4 byte aligned
*    - the number of bytes to be read is a multiple of 4
*    - the command reads data from memory array of the NOR flash (that is it has a 3 or 4 byte address)
*    - the read command is supported by the QSPI peripheral
*/
static U8 _IsDMATransferAllowed(const U8 * pData, unsigned NumBytesData, unsigned NumBytesAddr) {
  U8 r;

  r = 0;                                                    // Set to indicate that read via DMA is not allowed.
  if ((((U32)pData) & 0xE0000000uL) == 0x20000000uL) {      // The buffer has to be located in RAM (this is also required for the write operations)
    if (((U32)pData & 3u) == 0u) {                          // The address of the data has to be 4-byte aligned.
      if ((NumBytesData & 3u) == 0u) {                      // The number of bytes to be read has to be a multiple of 4.
        if ((NumBytesAddr == 3) || (NumBytesAddr == 4)) {   // The read operation has to use an address.
          r = 1;                                            // Read operation via DMA is allowed.
        }
      }
    }
  }
  return r;
}

/*********************************************************************
*
*       _ConfigDMAReadMode
*/
static int _ConfigDMAReadMode(U8 ReadMode, U8 Is4ByteAddr) {
  U32 ReadModeActual;
  U8  Is4ByteAddrActual;
  U8  IsReInitRequired;
  int r;
  int Result;

  r                = 0;
  IsReInitRequired = 0;
  ReadModeActual = (QSPI_IFCONFIG0 >> IFCONFIG0_READOC_BIT) & IFCONFIG0_READOC_MASK;
  if (ReadModeActual != (U32)ReadMode) {
    IsReInitRequired = 1;
  }
  Is4ByteAddrActual = 0;
  if (QSPI_IFCONFIG0 & (1uL << IFCONFIG0_ADDRMODE_BIT)) {
    Is4ByteAddrActual = 1;
  }
  if (Is4ByteAddrActual != Is4ByteAddr) {
    IsReInitRequired = 1;
  }
  if (IsReInitRequired != 0) {
    //
    // The QSPI peripheral uses the read mode configured
    // at initialization therefore when the read mode changes
    // we have to reinitialize the peripheral again.
    //
    _DeInit();
    _DMAReadMode = ReadMode;
    _Is4ByteAddr = Is4ByteAddr;
    Result = _Init();
    if (Result == 0) {
      r = 1;
    }
  }
  return r;
}

/*********************************************************************
*
*       _ReadDataViaDMA
*/
static int _ReadDataViaDMA(U32 ReadMode, const U8 * pAddr, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData) {
  U32 Addr;
  U8  Is4ByteAddr;
  U32 NumBytesAtOnce;
  int r;

  r = 0;
  Is4ByteAddr = 0;
  if (NumBytesAddr == 4) {
    Is4ByteAddr = 1;
  }
  //
  // Decode the address we have to read from.
  //
  if (Is4ByteAddr == 0) {
    Addr  = (U32)*pAddr++ << 16;
    Addr |= (U32)*pAddr++ << 8;
    Addr |= (U32)*pAddr;
  } else {
    Addr  = (U32)*pAddr++ << 24;
    Addr |= (U32)*pAddr++ << 16;
    Addr |= (U32)*pAddr++ << 8;
    Addr |= (U32)*pAddr;
  }
  if (NumBytesData != 0u) {
    do {
      NumBytesAtOnce = SEGGER_MIN(NumBytesData, MAX_READ_CNT);
      r = _ConfigDMAReadMode(ReadMode, Is4ByteAddr);
      if (r != 0) {
        break;
      }
      QSPI_READ_CNT         = NumBytesAtOnce;
      QSPI_READ_DST         = (U32)pData;
      QSPI_READ_SRC         = Addr;
      QSPI_EVENTS_READY     = 0;      // Clear event.
      QSPI_TASKS_READ_START = 1;      // Start the read operation.
      r = _WaitForReady();            // Wait for the read operation to finish.
      if (r != 0) {
        break;
      }
      NumBytesData -= NumBytesAtOnce;
      pData        += NumBytesAtOnce;
      Addr         += NumBytesAtOnce;
    } while (NumBytesData != 0u);
  }
  return r;
}

/*********************************************************************
*
*       _WriteDataViaCPU
*/
static int _WriteDataViaCPU(U8 Cmd, const U8 * pPara, unsigned NumBytesPara, const U8 * pData, unsigned NumBytesData) {
  U8  IsMoreData;
  int r;

  IsMoreData = 0;
  if (NumBytesData != 0u) {
    IsMoreData = 1;
  }
  r = _SendCmdAndData(Cmd, pPara, NumBytesPara, IsMoreData);
  if (r == 0) {
    if (IsMoreData != 0u) {
      r = _SendData(pData, NumBytesData);
    }
  }
  return r;
}

/*********************************************************************
*
*      _GetDMAWriteMode
*/
static int _GetDMAWriteMode(U8 Cmd, U16 BusWidth) {
  int WriteMode;

  WriteMode = -1;                        // Set to indicate that write via DMA is not supported.
  //
  // Check that the command and the bus configuration matches
  // one of the read codes supported by the QSPI peripheral.
  //
  switch (Cmd) {
  case 0x02:          // Address and data are single SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 1)) {
      WriteMode = IFCONFIG0_WRITEOC_PP;
    }
    break;
  case 0xA2:          // Address is single SPI, data is dual SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 2)) {
      WriteMode = IFCONFIG0_WRITEOC_PP2O;
    }
    break;
  case 0x32:          // Address is single SPI, data is quad SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 1)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 4)) {
      WriteMode = IFCONFIG0_WRITEOC_PP4O;
    }
    break;
  case 0x38:          // Address and data are quad SPI.
    if (   (FS_BUSWIDTH_GET_ADDR(BusWidth) == 4)
        && (FS_BUSWIDTH_GET_DATA(BusWidth) == 4)) {
      WriteMode = IFCONFIG0_WRITEOC_PP4IO;
    }
    break;
  default:
    break;
  }
  return WriteMode;
}

/*********************************************************************
*
*       _ConfigDMAWriteMode
*/
static int _ConfigDMAWriteMode(U8 WriteMode, U8 Is4ByteAddr) {
  U32 WriteModeActual;
  U8  Is4ByteAddrActual;
  U8  IsReInitRequired;
  int r;
  int Result;

  r                = 0;
  IsReInitRequired = 0;
  WriteModeActual = (QSPI_IFCONFIG0 >> IFCONFIG0_WRITEOC_BIT) & IFCONFIG0_WRITEOC_MASK;
  if (WriteModeActual != (U32)WriteMode) {
    IsReInitRequired = 1;
  }
  Is4ByteAddrActual = 0;
  if (QSPI_IFCONFIG0 & (1uL << IFCONFIG0_ADDRMODE_BIT)) {
    Is4ByteAddrActual = 1;
  }
  if (Is4ByteAddrActual != Is4ByteAddr) {
    IsReInitRequired = 1;
  }
  if (IsReInitRequired != 0) {
    //
    // The QSPI peripheral uses the read mode configured
    // at initialization therefore when the read mode changes
    // we have to reinitialize the peripheral again.
    //
    _DeInit();
    _DMAWriteMode = WriteMode;
    _Is4ByteAddr  = Is4ByteAddr;
    Result = _Init();
    if (Result == 0) {
      r = 1;
    }
  }
  return r;
}

/*********************************************************************
*
*       _WriteDataViaDMA
*/
static int _WriteDataViaDMA(U32 WriteMode, const U8 * pAddr, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData) {
  U32 Addr;
  U8  Is4ByteAddr;
  U32 NumBytesAtOnce;
  int r;

  r = 0;
  Is4ByteAddr = 0;
  if (NumBytesAddr == 4) {
    Is4ByteAddr = 1;
  }
  //
  // Decode the address we have to write to.
  //
  if (Is4ByteAddr == 0) {
    Addr  = (U32)*pAddr++ << 16;
    Addr |= (U32)*pAddr++ << 8;
    Addr |= (U32)*pAddr;
  } else {
    Addr  = (U32)*pAddr++ << 24;
    Addr |= (U32)*pAddr++ << 16;
    Addr |= (U32)*pAddr++ << 8;
    Addr |= (U32)*pAddr;
  }
  if (NumBytesData != 0u) {
    do {
      NumBytesAtOnce = SEGGER_MIN(NumBytesData, MAX_WRITE_CNT);
      r = _ConfigDMAWriteMode(WriteMode, Is4ByteAddr);
      if (r != 0) {
        break;
      }
      QSPI_WRITE_CNT         = NumBytesAtOnce;
      QSPI_WRITE_DST         = Addr;
      QSPI_WRITE_SRC         = (U32)pData;
      QSPI_EVENTS_READY      = 0;     // Clear event.
      QSPI_TASKS_WRITE_START = 1;     // Start the read operation.
      r = _WaitForReady();            // Wait for the read operation to finish.
      if (r != 0) {
        break;
      }
      NumBytesData -= NumBytesAtOnce;
      pData        += NumBytesAtOnce;
      Addr         += NumBytesAtOnce;
    } while (NumBytesData != 0u);
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
  int r;

  FS_USE_PARA(Unit);
  r = _Init();
  return r;
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
  //
  // The HW layer does not use the memory mapped mode.
  //
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
  FS_USE_PARA(Unit);                // The MCU has only one QSPI peripheral.
  FS_USE_PARA(BusWidth);            // The QSPI peripheral sends custom commands in single SPI mode.
  (void)_SendCmd(Cmd);
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
  int ReadMode;
  U8  IsDMATransferAllowed;

  FS_USE_PARA(Unit);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BusWidth);
  IsDMATransferAllowed = _IsDMATransferAllowed(pData, NumBytesData, NumBytesAddr);
  if (IsDMATransferAllowed != 0u) {
    ReadMode = _GetDMAReadMode(Cmd, BusWidth);
    if (ReadMode < 0) {       // Read operation not supported via DMA?
      IsDMATransferAllowed = 0;
    }
  }
  if (IsDMATransferAllowed == 0u) {
    (void)_ReadDataViaCPU(Cmd, pPara, NumBytesPara, NumBytesAddr, pData, NumBytesData, BusWidth);
  } else {
    (void)_ReadDataViaDMA(ReadMode, pPara, NumBytesAddr, pData, NumBytesData);
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
  int WriteMode;
  U8  IsDMATransferAllowed;

  FS_USE_PARA(Unit);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BusWidth);
  IsDMATransferAllowed = _IsDMATransferAllowed(pData, NumBytesData, NumBytesAddr);
  if (IsDMATransferAllowed != 0u) {
    WriteMode = _GetDMAWriteMode(Cmd, BusWidth);
    if (WriteMode < 0) {       // Write operation not supported via DMA?
      IsDMATransferAllowed = 0;
    }
  }
  if (IsDMATransferAllowed == 0u) {
    (void)_WriteDataViaCPU(Cmd, pPara, NumBytesPara, pData, NumBytesData);
  } else {
    (void)_WriteDataViaDMA(WriteMode, pPara, NumBytesAddr, pData, NumBytesData);
  }
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_nRF52840_NordicSemi_nRF52840_DK = {
  _HW_Init,
  _HW_SetCmdMode,
  NULL,
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
