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

File    : FS_NOR_HW_SPI_STM32F103_ST_MB672.c
Purpose : NOR SPI hardware layer for STM3210E-EVAL board (Rev. D)
Literature:
  [1] RM0008 Reference manual STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx advanced ARM-based 32-bit MCUs
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32F1\STM32F10_RM.pdf)
  [2] UM0488 User manual STM3210E-EVAL evaluation board
    (\\fileserver\Techinfo\Company\ST\MCU\STM32\STM32F1\EvalBoard\MB672_STM32F103\STM3210E-EVAL_UM0488_Rev1.pdf)
---------------------------END-OF-HEADER------------------------------
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
#ifndef   FS_NOR_HW_PER_CLOCK_KHZ
  #define FS_NOR_HW_PER_CLOCK_KHZ     72000     // Clock frequency of SPI peripheral.
#endif

#ifndef   FS_NOR_HW_SPI_CLOCK_KHZ
  #define FS_NOR_HW_SPI_CLOCK_KHZ     18000     // SPI communication speed.
#endif

#ifndef   FS_NOR_HW_USE_DMA
  #define FS_NOR_HW_USE_DMA           0         // Set to 1 if DMA must be used for the data transfer.
#endif

#ifndef   FS_NOR_HW_USE_OS
  #define FS_NOR_HW_USE_OS            0         // Selects the operating mode: 1 event-driven, 0 polling.
#endif

/*********************************************************************
*
*       #include section, conditional
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  #include "RTOS.h"
  #include "stm32f10x.h"
  #include "FS_OS.h"
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       Reset and clock control
*/
#define RCC_BASE_ADDR     0x40021000uL
#define RCC_APB2RSTR      (*(volatile U32 *)(RCC_BASE_ADDR + 0x0C))
#define RCC_AHBENR        (*(volatile U32 *)(RCC_BASE_ADDR + 0x14))
#define RCC_APB2ENR       (*(volatile U32 *)(RCC_BASE_ADDR + 0x18))

/*********************************************************************
*
*       GPIO A registers
*/
#define GPIOA_BASE_ADDR   0x40010800uL
#define GPIOA_CRL         (*(volatile U32 *)(GPIOA_BASE_ADDR + 0x00))

/*********************************************************************
*
*       GPIO B registers
*/
#define GPIOB_BASE_ADDR   0x40010C00uL
#define GPIOB_CRL         (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x00))
#define GPIOB_BSRR        (*(volatile U32 *)(GPIOB_BASE_ADDR + 0x10))

/*********************************************************************
*
*       SPI peripheral registers
*/
#define SPI1_BASE_ADDR    0x40013000uL
#define SPI2_BASE_ADDR    0x40003800uL
#define SPI_BASE_ADDR     SPI1_BASE_ADDR
#define SPI_CR1           (*(volatile U32 *)(SPI_BASE_ADDR + 0x00))
#define SPI_CR2           (*(volatile U32 *)(SPI_BASE_ADDR + 0x04))
#define SPI_SR            (*(volatile U32 *)(SPI_BASE_ADDR + 0x08))
#define SPI_DR            (*(volatile U32 *)(SPI_BASE_ADDR + 0x0C))

/*********************************************************************
*
*       DMA registers
*/
#define DMA1_BASE_ADDR    0x40020000uL
#define DMA2_BASE_ADDR    0x40020400uL
#define DMA_BASE_ADDR     DMA1_BASE_ADDR
#define DMA_ISR           (*(volatile U32 *)(DMA_BASE_ADDR + 0x00))
#define DMA_IFCR          (*(volatile U32 *)(DMA_BASE_ADDR + 0x04))
#define DMA_CCR1          (*(volatile U32 *)(DMA_BASE_ADDR + 0x08))
#define DMA_CNDTR1        (*(volatile U32 *)(DMA_BASE_ADDR + 0x0C))
#define DMA_CPAR1         (*(volatile U32 *)(DMA_BASE_ADDR + 0x10))
#define DMA_CMAR1         (*(volatile U32 *)(DMA_BASE_ADDR + 0x14))
#define DMA_CCR2          (*(volatile U32 *)(DMA_BASE_ADDR + 0x1C))
#define DMA_CNDTR2        (*(volatile U32 *)(DMA_BASE_ADDR + 0x20))
#define DMA_CPAR2         (*(volatile U32 *)(DMA_BASE_ADDR + 0x24))
#define DMA_CMAR2         (*(volatile U32 *)(DMA_BASE_ADDR + 0x28))
#define DMA_CCR3          (*(volatile U32 *)(DMA_BASE_ADDR + 0x30))
#define DMA_CNDTR3        (*(volatile U32 *)(DMA_BASE_ADDR + 0x34))
#define DMA_CPAR3         (*(volatile U32 *)(DMA_BASE_ADDR + 0x38))
#define DMA_CMAR3         (*(volatile U32 *)(DMA_BASE_ADDR + 0x3C))

/*********************************************************************
*
*       SPI status flags
*/
#define SR_RXNE           0
#define SR_TXE            1
#define SR_BSY            7

/*********************************************************************
*
*       SPI control flags
*/
#define CR2_RXDMAEN       0
#define CR2_TXDMAEN       1

/*********************************************************************
*
*      DMA interrupt clear flags
*/
#define IFCR_CGIF2        4
#define IFCR_CGIF3        8

/*********************************************************************
*
*      DMA interrupt status flags
*/
#define ISR_TCIF1         1
#define ISR_TEIF1         3
#define ISR_GIF2          4
#define ISR_TCIF2         5
#define ISR_TEIF2         7
#define ISR_GIF3          8
#define ISR_TCIF3         9
#define ISR_TEIF3         11

/*********************************************************************
*
*      DMA configuration flags
*/
#define CCR_EN            0
#define CCR_TCIE          1
#define CCR_TEIE          3
#define CCR_DIR           4
#define CCR_MINC          7
#define CCR_PSIZE         8
#define CCR_MSIZE         10

/*********************************************************************
*
*       Misc. defines
*/
#define AHBENR_DMA1EN     0
#define AHBENR_DMA2EN     1
#define NOR_CS_BIT        2
#define WAIT_TIMEOUT_MAX  1000
#define DMA_PRIO          15

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
#if FS_NOR_HW_USE_OS
  volatile U32 _StatusDMACh2;
  volatile U32 _StatusDMACh3;
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _SetMaxSpeed
*/
static U16 _SetMaxSpeed(U8 Unit, U16 MaxFreq) {
  U32      Div;
  unsigned DivVal;

  FS_USE_PARA(Unit);
  Div    = 2;
  DivVal = 0;
  while ((MaxFreq * Div) < FS_NOR_HW_PER_CLOCK_KHZ) {
    Div <<= 1;
    if (++DivVal == 7) {
      break;
    }
  }
  //
  // Disable the SPI initially before setting the new divider
  //
  SPI_CR1 &= ~(1uL << 6);
  //
  // Set the divider
  //
  SPI_CR1 &= ~(7uL << 3);
  SPI_CR1 |= (DivVal << 3);
  //
  // Enable SPI
  //
  SPI_CR1 |= (1 << 6);
  return (U16)(FS_NOR_HW_PER_CLOCK_KHZ / Div);    // We are not faster than this.
}

#if (FS_NOR_HW_USE_DMA == 0)

/*********************************************************************
*
*       _ReadWriteSPI
*/
static U8 _ReadWriteSPI(U8 Data) {
  //
  // Send data.
  //
  SPI_DR = Data;
  //
  // Wait until all bits are shifted.
  //
  while (0 == (SPI_SR & (1uL << SR_RXNE)));
  //
  // Read data.
  //
  Data = (U8)SPI_DR;
  return Data;
}

#endif

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

#if FS_NOR_HW_USE_OS

/**********************************************************
*
*       DMA1_Channel2_IRQHandler
*
*   Function description
*     Handles the DMA interrupt which moves the data from NOR flash to memory.
*/
void DMA1_Channel2_IRQHandler(void);
void DMA1_Channel2_IRQHandler(void) {
  OS_EnterInterrupt();              // Inform embOS that interrupt code is running
  _StatusDMACh2 = DMA_ISR;          // Remember the status for the waiting task.
  DMA_IFCR = 1uL << IFCR_CGIF2;
  FS_X_OS_Signal();                 // Unblock the task which is waiting on interrupt.
  OS_LeaveInterrupt();              // Inform embOS that interrupt code is left
}

/**********************************************************
*
*       DMA1_Channel3_IRQHandler
*
*   Function description
*     Handles the DMA interrupt which moves the data from memory to NOR flash.
*/
void DMA1_Channel3_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void) {
  OS_EnterInterrupt();              // Inform embOS that interrupt code is running
  _StatusDMACh3 = DMA_ISR;          // Remember the status for the waiting task.
  DMA_IFCR = 1uL << IFCR_CGIF3;
  FS_X_OS_Signal();                 // Unblock the task which is waiting on interrupt.
  OS_LeaveInterrupt();              // Inform embOS that interrupt code is left
}

#endif

/*********************************************************************
*
*       Public code, via callback
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
*    Unit   Device index.
*
*  Return value
*    SPI frequency that is set - given in kHz.
*
*/
static int _HW_Init(U8 Unit) {
  U32 v;
  int Freq_kHz;

  FS_USE_PARA(Unit);
  RCC_APB2RSTR &= ~(1uL << 2);      // Remove RESET from PortA
  RCC_APB2ENR  |=  (1uL << 2);      // Enable clock for PortA
  RCC_APB2RSTR &= ~(1uL << 3);      // Remove RESET from PortB
  RCC_APB2ENR  |=  (1uL << 3);      // Enable clock for PortB
  RCC_APB2RSTR &= ~(1uL << 12);     // Remove RESET from SPI
  RCC_APB2ENR  |=  (1uL << 12);     // Enable clock for SPI
  //
  // Reset port pins to reset state
  //
  v  = GPIOB_CRL;
  v &= ~0x00000F00uL;
  v |= (3 << 8)     // PortB02 -> GPIO output, max 50MHz
    |  (0 << 10)    // PortB02 -> Output mode, using PP (used for CS)
    ;
  GPIOB_CRL = v;
  v  = GPIOA_CRL;
  v &= ~0xFFFF0000;
  v |= (3uL << 20)  // PortA05 -> GPIO output, max 50MHz
    |  (2uL << 22)  // PortA05 -> Output mode, using PP (used for SPI_CLK)
    |  (3uL << 24)  // PortA06 -> GPIO input
    |  (3uL << 26)  // PortA06 -> Input mode, floating (used for SPI_MISO)
    |  (3uL << 28)  // PortA07 -> GPIO output, max 50MHz
    |  (2uL << 30)  // PortA07 -> Output mode, using PP (used for SPI_MOSI)
    ;
  GPIOA_CRL = v;
  //
  // Initialize SPI
  //
  SPI_CR1  = (0 << 0)         // Second clock transition is the first data capture edge
           | (0 << 1)         // Clock polarity is high when idle
           | (1 << 2)         // SPI is master
           | (7 << 3)         // Clock = pclk (72MHz) / 256
           | (1 << 8)
           | (1 << 9)
           ;
  SPI_CR2  = 0;               // Neither interrupts nor DMA is used
  SPI_CR1 |= (1 << 6)         // Enable SPI
           ;
#if FS_NOR_HW_USE_DMA
  //
  // Power up the DMA module.
  //
  RCC_AHBENR |= (1uL << AHBENR_DMA1EN);
  //
  // Initialize the DMA channels used by SPI.
  //
  DMA_IFCR  = 0
            | (1uL << IFCR_CGIF2)
            | (1uL << IFCR_CGIF3)
            ;
  SPI_CR2  |= (1uL << CR2_TXDMAEN)
           |  (1uL << CR2_RXDMAEN)
           ;
#if FS_NOR_HW_USE_OS
  //
  // Set the priority and enable the interrupts.
  //
  NVIC_SetPriority(DMA1_Channel2_IRQn, DMA_PRIO);
  NVIC_SetPriority(DMA1_Channel3_IRQn, DMA_PRIO);
  NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  NVIC_EnableIRQ(DMA1_Channel2_IRQn);
#endif
#endif
  Freq_kHz = (int)_SetMaxSpeed(Unit, FS_NOR_HW_SPI_CLOCK_KHZ);
  return Freq_kHz;
}

/*********************************************************************
*
*       _HW_EnableCS
*
*  Function description
*    Activates the SD card  using the chip select (CS) line.
*
*  Parameters
*    Unit     Device index.
*/
static void _HW_EnableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  GPIOB_BSRR = 1uL << (NOR_CS_BIT + 16);
}

/*********************************************************************
*
*       _HW_DisableCS
*
*  Function description
*    Deactivates the SD card using the chip select (CS) line.
*
*  Parameters
*    Unit     Device index.
*/
static void _HW_DisableCS(U8 Unit) {
  FS_USE_PARA(Unit);
  GPIOB_BSRR = 1uL << NOR_CS_BIT;
}

/*********************************************************************
*
*       _HW_Read
*
*  Function description
*    Reads a specified number of bytes from flash to buffer.
*
*  Parameters
*    Unit       Device index.
*    pData      Pointer to a data buffer.
*    NumBytes   Number of bytes.
*/
static void _HW_Read(U8 Unit, U8 * pData, int NumBytes) {
#if FS_NOR_HW_USE_DMA
  volatile U32 Dummy;

  FS_USE_PARA(Unit);
  Dummy = 0;
  FS_USE_PARA(Dummy);
  //
  // Clear transfer errors.
  //
  Dummy = SPI_DR;
  Dummy = SPI_SR;
#if FS_NOR_HW_USE_OS
  _StatusDMACh2 = 0;
  _StatusDMACh3 = 0;
#endif
  //
  // Setup the DMA transfer. One DMA channel is used for sending and the other one for receiving.
  //
  DMA_CCR2    = 0;                      // The DMA channel can be configured only when disabled.
  DMA_CPAR2   = (U32)&SPI_DR;
  DMA_CMAR2   = (U32)pData;
  DMA_CNDTR2  = (U32)NumBytes;
  DMA_CCR2   &= ~(1uL << CCR_DIR);      // Data is transfered from peripheral to memory.
  DMA_CCR2   |= 1uL << CCR_MINC;        // Memory address is incremented
  DMA_IFCR    = 1uL << IFCR_CGIF2;      // Clear all the pending interrupt flags.
#if FS_NOR_HW_USE_OS
  DMA_CCR2 |= 0                         // Enable the DMA interrupts.
           |  (1uL << CCR_TCIE)
           |  (1uL << CCR_TEIE)
           ;
#endif
  DMA_CCR2   |= 1uL << CCR_EN;          // Start receiving.

  Dummy       = 0xFFFFFFFF;
  DMA_CCR3    = 0;                      // The DMA channel can be configured only when disabled.
  DMA_CPAR3   = (U32)&SPI_DR;
  DMA_CMAR3   = (U32)&Dummy;
  DMA_CNDTR3  = (U32)NumBytes;
  DMA_CCR3   |= 1uL << CCR_DIR;         // Data is transfered from memory to peripheral.
  DMA_IFCR    = (1uL << IFCR_CGIF3);    // Clear all the pending interrupt flags.
#if FS_NOR_HW_USE_OS
  DMA_CCR3 |= 0                         // Enable the DMA interrupts.
           |  (1uL << CCR_TEIE)
           ;
#endif
  DMA_CCR3   |= 1uL << CCR_EN;          // Start sending
  //
  // Wait for DMA transfer to complete.
  //
  while (1) {
    U32 Status;

#if FS_NOR_HW_USE_OS
    FS_X_OS_Wait(WAIT_TIMEOUT_MAX);
    Status  = _StatusDMACh2;
    Status |= _StatusDMACh3;
#else
    Status = DMA_ISR;
#endif  // FS_NOR_HW_USE_OS
    if (Status & ((1uL << ISR_TEIF2) | (1uL << ISR_TEIF3))) {
      break;                            // Error, data transfer failed.
    }
    if (Status & (1uL << ISR_TCIF2)) {
      break;                            // OK, data read from SD card.
    }
  }
  //
  // Transfer complete, disable the DMA channels.
  //
  DMA_CCR2 = 0;
  DMA_CCR3 = 0;
#else
  FS_USE_PARA(Unit);
  do {
    *pData++ = _ReadWriteSPI(0xff);
  } while (--NumBytes);
#endif
}

/*********************************************************************
*
*       _HW_Write
*
*  Function description
*    Writes a specified number of bytes from data buffer to flash.
*
*  Parameters
*    Unit       Device index.
*    pData      Pointer to a data buffer.
*    NumBytes   Number of bytes.
*/
static void _HW_Write(U8 Unit, const U8 * pData, int NumBytes) {
#if FS_NOR_HW_USE_DMA
  volatile U32 Dummy;

  FS_USE_PARA(Unit);
  Dummy = 0;
  FS_USE_PARA(Dummy);
  //
  // Clear transfer errors.
  //
  Dummy = SPI_DR;
  Dummy = SPI_SR;
#if FS_NOR_HW_USE_OS
  _StatusDMACh2 = 0;
  _StatusDMACh3 = 0;
#endif
  //
  // Setup the DMA transfer. One DMA channel is used for sending and the other one for receiving.
  //
  DMA_CCR2    = 0;                      // The DMA channel can be configured only when disabled.
  DMA_CPAR2   = (U32)&SPI_DR;
  DMA_CMAR2   = (U32)&Dummy;
  DMA_CNDTR2  = (U32)NumBytes;
  DMA_IFCR    = 1uL << IFCR_CGIF2;      // Clear all the pending interrupt flags.
#if FS_NOR_HW_USE_OS
  DMA_CCR2 |= 0                         // Enable the DMA interrupts.
           |  (1uL << CCR_TCIE)
           |  (1uL << CCR_TEIE)
           ;
#endif  // FS_NOR_HW_USE_OS
  DMA_CCR2   |= 1uL << CCR_EN;          // Start receiving.

  DMA_CCR3    = 0;                      // The DMA channel can be configured only when disabled.
  DMA_CPAR3   = (U32)&SPI_DR;
  DMA_CMAR3   = (U32)pData;
  DMA_CNDTR3  = (U32)NumBytes;
  DMA_CCR3   |= 1uL << CCR_DIR;         // Data is transfered from memory to peripheral.
  DMA_CCR3   |= 1uL << CCR_MINC;        // Memory address is incremented
  DMA_IFCR    = 1uL << IFCR_CGIF3;      // Clear all the pending interrupt flags.
#if FS_NOR_HW_USE_OS
  DMA_CCR3 |= 0                         // Enable the DMA interrupts.
           |  (1uL << CCR_TEIE)
           ;
#endif  // FS_NOR_HW_USE_OS
  DMA_CCR3   |= 1uL << CCR_EN;          // Start sending data.
  //
  // Wait for DMA transfer to complete.
  //
  while (1) {
    U32 Status;

#if FS_NOR_HW_USE_OS
    FS_X_OS_Wait(WAIT_TIMEOUT_MAX);
    Status  = _StatusDMACh2;
    Status |= _StatusDMACh3;
#else
    Status = DMA_ISR;
#endif  // FS_NOR_HW_USE_OS
    if (Status & ((1uL << ISR_TEIF2) | (1uL << ISR_TEIF3))) {
      break;                            // Error, data transfer failed.
    }
    if (Status & (1uL << ISR_TCIF2)) {
      break;                            // OK, data read from SD card.
    }
  }
  //
  // Transfer complete, disable the DMA channels.
  //
  DMA_CCR2 = 0;
  DMA_CCR3 = 0;
#else
  FS_USE_PARA(Unit);
  do {
    _ReadWriteSPI(*pData++);
  } while (--NumBytes);
#endif  // FS_NOR_HW_USE_DMA
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPI FS_NOR_HW_SPI_STM32F103_ST_MB672 = {
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
