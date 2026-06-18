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

File    : FS_NOR_SPI_Template.c
Purpose : Template hardware layer for single SPI serial NOR flash.
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS.h"
#include "FS_NOR_HW_SPI_Template.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
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
  SPIFreq_kHz = 0x40000000;            // SPI speed=40 MHz as per the docklight

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
*       _HW_Erase
*
*  Function description
*    Erase data sector
*
*  Parameters
*    Sector_Number      Base_address of the sector
*    NumBytes  		   Number of bytes to be erased
*/

void Erase_sector(unsigned int Sector_Number)
{
	spi_flash_erase_sector(Sector_Number);
}


/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Transfers data from serial NOR flash device to MCU.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    pData      Transferred data.
*    NumBytes   Number of bytes to transfer.
*/

static void _HW_Read(U8 Unit, U8 * pData, int NumBytes, unsigned int address) {

	//printf("\n Hardware Read \n");

  FS_USE_PARA(Unit);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytes);
  spi_flash_read(address, pData, NumBytes);
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Transfers data from MCU to serial NOR flash device.
*
*  Parameters
*    Unit       Index of the hardware layer (0-based).
*    pData      Data to be transferred.
*    NumBytes   Number of bytes to be transferred.
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes,unsigned int address) {
//	printf("\n Hardware write \n");
  FS_USE_PARA(Unit);
  FS_USE_PARA(pData);
  FS_USE_PARA(NumBytes);
  spi_flash_write(address, pData, NumBytes);
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
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_Template = {
  _HW_Init,
  _HW_EnableCS,
  _HW_DisableCS,
  _HW_Read,
  _HW_Write,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,	 	//  _HW_Delay,
  NULL, 	//  _HW_Lock,
  NULL, 	//  _HW_Unlock,
  NULL,
  NULL
};

/*************************** End of file ****************************/
