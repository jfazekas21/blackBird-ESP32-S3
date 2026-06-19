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
Purpose : Template hardware layer for memory mapped single/dual/quad SPI NOR flash.
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
*  Function description
*    HW layer function. It is called before any other function of the physical layer.
*    It should configure the HW so that the other functions can access the NOR flash.
*
*  Return value
*    Frequency of the SPI clock in Hz.
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
*    HW layer function. It enables the direct access to NOR flash via SPI.
*    This function disables the memory-mapped mode.
*/
static void _HW_Unmap(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Map
*
*  Function description
*    HW layer function. It enables the memory-mapped mode. In this mode
*    the data can be accessed by doing read operations from memory.
*    The HW is responsible to transfer the data via SPI.
*    This function disables the direct access to NOR flash via SPI.
*/
static void _HW_Map(U8 Unit, U8 ReadCmd, unsigned NumBytesAddr, unsigned NumBytesDummy, U16 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(ReadCmd);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(NumBytesDummy);
  FS_USE_PARA(BusWidth);
}

/*********************************************************************
*
*       _HW_Control
*
*  Function description
*    HW layer function. It requests the NOR flash to execute a simple command.
*    The HW has to be in SPI mode.
*/
static void _HW_Control(U8 Unit, U8 Cmd, U8 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(Cmd);
  FS_USE_PARA(BusWidth);
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    HW layer function. It transfers data from NOR flash to MCU.
*    The HW has to be in SPI mode.
*/
static void _HW_Read(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(Cmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytesData);
  FS_USE_PARA(BusWidth);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    HW layer function. It transfers data from MCU to NOR flash.
*    The HW has to be in SPI mode.
*/
static void _HW_Write(U8 Unit, U8 Cmd, const U8 * pPara, unsigned NumBytesPara, unsigned NumBytesAddr, const U8 * pData, unsigned NumBytesData, U16 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(Cmd);
  FS_USE_PARA(pPara);
  FS_USE_PARA(NumBytesPara);
  FS_USE_PARA(NumBytesAddr);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytesData);
  FS_USE_PARA(BusWidth);
}

/*********************************************************************
*
*       _HW_Poll
*
*  Function description
*    HW layer function. Sends a command repeatedly and checks the
*    response for a specified condition.
*
*  Return value
*    > 0    Timeout occurred.
*    ==0    OK, bit set to specified value.
*    < 0    Feature not supported.
*
*  Additional information
*    The function executes periodically a command and waits
*    until specified bit in the response returned by the NOR flash
*    is set to a value specified by BitValue. The position of the bit
*    that has to be checked is specified by BitPos where 0 is the
*    position of the least significant bit in the byte.
*/
static int _HW_Poll(U8 Unit, U8 Cmd, U8 BitPos, U8 BitValue, U32 Delay, U32 TimeOut_ms, U16 BusWidth) {
  int r;

  FS_USE_PARA(Unit);
  FS_USE_PARA(Cmd);
  FS_USE_PARA(BitPos);
  FS_USE_PARA(BitValue);
  FS_USE_PARA(Delay);
  FS_USE_PARA(TimeOut_ms);
  FS_USE_PARA(BusWidth);
  r = -1;                 // Set to indicate that the feature is not supported.
  return r;
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
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_Template = {
  _HW_Init,
  _HW_Unmap,
  _HW_Map,
  _HW_Control,
  _HW_Read,
  _HW_Write,
  _HW_Poll,
  _HW_Delay,
  _HW_Lock,
  _HW_Unlock,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*************************** End of file ****************************/
