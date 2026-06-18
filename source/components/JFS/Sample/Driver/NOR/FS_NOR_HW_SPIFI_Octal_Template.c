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

File    : FS_NOR_HW_SPIFI_Template.c
Purpose : Template hardware layer for memory mapped octal SPI NOR flash.
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

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

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
  FS_USE_PARA(Unit);
  return 0;
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
*
*  Additional information
*    This function disables the memory-mapped mode.
*/
static void _HW_Unmap(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Delay
*
*  Function description
*    HW layer function. Blocks the execution for the specified number
*    of milliseconds.
*
*  Return value
*    ==0    Delay executed.
*    < 0    Feature not supported.
*/
static int _HW_Delay(U8 Unit, U32 ms) {
  int r;

  FS_USE_PARA(Unit);
  FS_USE_PARA(ms);
  r = -1;                 // Set to indicate that the feature is not supported.
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(pCmd);
  FS_USE_PARA(NumBytesCmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(BusWidth);
  FS_USE_PARA(Flags);
  return 0;
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(pCmd);
  FS_USE_PARA(NumBytesCmd);
  FS_USE_PARA(BusWidth);
  FS_USE_PARA(Flags);
  return 0;
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(pCmd);
  FS_USE_PARA(NumBytesCmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytesData);
  FS_USE_PARA(BusWidth);
  FS_USE_PARA(Flags);
  return 0;
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
  FS_USE_PARA(Unit);
  FS_USE_PARA(pCmd);
  FS_USE_PARA(NumBytesCmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytesData);
  FS_USE_PARA(BusWidth);
  FS_USE_PARA(Flags);
  return 0;
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
  return r;
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_Octal_Template = {
  _HW_Init,
  _HW_Unmap,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  _HW_Delay,
  _HW_Lock,
  _HW_Unlock,
  _HW_MapEx,
  _HW_ControlEx,
  _HW_ReadEx,
  _HW_WriteEx,
  _HW_PollEx
};

/*************************** End of file ****************************/
