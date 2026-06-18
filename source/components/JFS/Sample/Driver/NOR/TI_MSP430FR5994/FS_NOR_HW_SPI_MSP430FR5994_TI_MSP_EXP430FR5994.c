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

File    : FS_NOR_HW_SPI_MSP430FR5994_TI_MSP_EXP430FR5994.c
Purpose : NOR SPI hardware layer for TI MSP430FR5994.
Additional information:

  The NOR flash device is soldered on an adapter board that is connected
  to the evaluation board via patch wires. The following table lists
  how the signals are connected.

  NOR flash         Evaluation
  SOP16             board
  ----------------------------
  1.  HOLD#/IO3     P8.0 (GPIO)
  2.  VCC           +3.3V
  3.  RESET#        P6.3 (GPIO)
  4.  n/c
  5.  n/c
  6.  n/c
  7.  CS#           P6.2 (GPIO)
  8.  SO/IO1        P5.1 (UCB1SOMI)
  9.  WP#/IO2       P4.7 (GPIO)
  10. VSS           GND
  11. n/c
  12. n/c
  13. n/c
  14. VIO           n/c
  15. SI/I0         P5.0 (UCB1SIMO)
  16. SCK           P5.2 (UCB1CLK)
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stddef.h>
#include "driverlib.h"
#include "FS.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
#define SPI_CLK_FREQ_HZ           8000000uL             // Clock of SPI peripheral in Hz
#define NOR_CLK_FREQ_HZ           (SPI_CLK_FREQ_HZ / 2) // Clock frequency supplied to NOR flash device.

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       Public code
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
  int                         SPIFreq_kHz;
  EUSCI_B_SPI_initMasterParam Param;

  FS_USE_PARA(Unit);
  //
  // Configure the CLK pin.
  //
  GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P5, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);
  //
  // Configure the MOSI pin.
  //
  GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P5, GPIO_PIN0, GPIO_PRIMARY_MODULE_FUNCTION);
  //
  // Configure the MISO pin.
  //
  GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5, GPIO_PIN1, GPIO_PRIMARY_MODULE_FUNCTION);
  //
  // Configure the CS pin.
  //
  GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN2);
  GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN2);
  //
  // Configure the RESET pin.
  //
  GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN3);
  GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN3);
  //
  // Configure the HOLD pin.
  //
  GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN0);
  GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN0);
  //
  // Configure the WP pin.
  //
  GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN7);
  GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN7);
  //
  // Initialize the SPI module.
  //
  memset(&Param, 0, sizeof(Param));
  Param.selectClockSource    = EUSCI_B_SPI_CLOCKSOURCE_SMCLK;
  Param.clockSourceFrequency = SPI_CLK_FREQ_HZ;
  Param.desiredSpiClock      = NOR_CLK_FREQ_HZ;
  Param.msbFirst             = EUSCI_B_SPI_MSB_FIRST;
  Param.clockPhase           = EUSCI_B_SPI_PHASE_DATA_CHANGED_ONFIRST_CAPTURED_ON_NEXT;
  Param.clockPolarity        = EUSCI_B_SPI_CLOCKPOLARITY_INACTIVITY_HIGH;
  Param.spiMode              = EUSCI_B_SPI_3PIN;
  EUSCI_B_SPI_initMaster(EUSCI_B1_BASE, &Param);
  //
  // Enable the SPI module.
  //
  EUSCI_B_SPI_enable(EUSCI_B1_BASE);
  //
  // Clear the RX flag.
  //
  EUSCI_B_SPI_clearInterrupt(EUSCI_B1_BASE, EUSCI_B_SPI_RECEIVE_INTERRUPT);
  SPIFreq_kHz = (int)(NOR_CLK_FREQ_HZ / 1000u);
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
*    Unit   Device index
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN2);
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
  GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN2);
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
  uint8_t Status;

  FS_USE_PARA(Unit);
  //
  // Clear the RX status flag.
  //
  EUSCI_B_SPI_clearInterrupt(EUSCI_B1_BASE, EUSCI_B_SPI_RECEIVE_INTERRUPT);
  if (NumBytes > 0) {
    do {
      //
      // Wait while not ready for TX.
      //
      for (;;) {
        Status = EUSCI_B_SPI_getInterruptStatus(EUSCI_B1_BASE, EUSCI_B_SPI_TRANSMIT_INTERRUPT);
        if (Status != 0) {
          break;
        }
      }
      //
      // Perform the a data transfer by writing a dummy byte.
      //
      EUSCI_B_SPI_transmitData(EUSCI_B1_BASE, 0xFF);
      //
      // Wait for RX buffer full.
      //
      for (;;) {
        Status = EUSCI_B_SPI_getInterruptStatus(EUSCI_B1_BASE, EUSCI_B_SPI_RECEIVE_INTERRUPT);
        if (Status != 0) {
          break;
        }
      }
      *pData++ = EUSCI_B_SPI_receiveData(EUSCI_B1_BASE);
    } while (--NumBytes != 0);
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
  uint8_t Status;

  FS_USE_PARA(Unit);
  if (NumBytes > 0) {
    do {
      //
      // Wait for TX ready.
      //
      for (;;) {
        Status = EUSCI_B_SPI_getInterruptStatus(EUSCI_B1_BASE, EUSCI_B_SPI_TRANSMIT_INTERRUPT);
        if (Status != 0) {
          break;
        }
      }
      //
      // Send data to NOR flash device.
      //
      EUSCI_B_SPI_transmitData(EUSCI_B1_BASE, *pData++);
    } while (--NumBytes != 0);
  }
  //
  // Wait for the data transfer to finish.
  //
  for (;;) {
    if (EUSCI_B_SPI_isBusy(EUSCI_B1_BASE) == 0u) {
      break;
    }
  }
  //
  // Perform a dummy read to empty RX buffer
  // and clear any overrun conditions.
  //
  EUSCI_B_SPI_receiveData(EUSCI_B1_BASE);
}

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_MSP430FR5994_TI_MSP_EXP430FR5994 = {
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
