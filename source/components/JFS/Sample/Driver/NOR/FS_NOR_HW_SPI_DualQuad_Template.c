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

File    : FS_NOR_SPI_DualQuad_Template.c
Purpose : Template hardware layer for dual/quad SPI NOR flash.
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
*    Initialize the SPI for use with the NOR flash.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*
*  Return value
*    !=0    Configured SPI clock frequency kHz.
*    ==0    An error occurred.
*/
static int _HW_Init(U8 Unit) {
  int SPIFreq_kHz;

  FS_USE_PARA(Unit);
  SPIFreq_kHz = 0;
  //
  // TBD: Insert initialization code here.
  //
  return SPIFreq_kHz;
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Activates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Deactivates chip select signal (CS) of the NOR flash chip.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_DisableCS(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Delay
*
*  Function description
*    Blocks the execution for the specified time.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    ms         Number of milliseconds to block the execution.
*
*  Return value
*    ==0    OK, delay executed.
*    < 0    Functionality not supported.
*/
static int _HW_Delay(U8 Unit, U32 ms) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(ms);
  return -1;              // Set to indicated that the functionality is not supported.
}

/*********************************************************************
*
*       _HW_Lock
*
*  Function description
*    Requests exclusive access to SPI bus.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_Lock(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_Unlock
*
*  Function description
*    Requests exclusive access to SPI bus.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*/
static void _HW_Unlock(U8 Unit) {
  FS_USE_PARA(Unit);
}

/*********************************************************************
*
*       _HW_ReadEx
*
*  Function description
*    Transfers data from serial NOR flash device to MCU.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    pData      Transferred data.
*    NumBytes   Number of bytes to be transferred.
*    BusWidth   Number of data lines to be used for the transfer.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_ReadEx(U8 Unit, U8 * pData, U32 NumBytes, U8 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytes);
  FS_USE_PARA(BusWidth);
  return 0;
}

/*********************************************************************
*
*       _HW_WriteEx
*
*  Function description
*    Transfers data from MCU to serial NOR flash device.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    pData      Data to be transferred.
*    NumBytes   Number of bytes to be transferred.
*    BusWidth   Number of data lines to be used for the transfer.
*
*  Return value
*    ==0      OK, data transferred successfully.
*    !=0      An error occurred.
*/
static int _HW_WriteEx(U8 Unit, const U8 * pData, U32 NumBytes, U8 BusWidth) {
  FS_USE_PARA(Unit);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytes);
  FS_USE_PARA(BusWidth);
  return 0;
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_DualQuad_Template = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  _HW_Delay,
  _HW_Lock,
  _HW_Unlock,
  _HW_ReadEx,
  _HW_WriteEx
};

/*************************** End of file ****************************/
