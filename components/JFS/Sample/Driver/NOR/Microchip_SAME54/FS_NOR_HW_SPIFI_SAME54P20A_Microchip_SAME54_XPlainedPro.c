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

File    : FS_NOR_HW_SPIFI_SAME54P20A_Microchip_SAME54_XPlainedPro.c
Purpose : Low-level driver for Microchip SAME54 and quad SPI NOR flash.
Literature:
  [1] Data sheet 32-bit ARM-based Microcontroller SAM D51 / SAM E51 / SAM E53 / SAM E54
    (\\fileserver\Techinfo\Company\Microchip\MCU\ARM\ATSAME54\SAM-D51-E51-E53-E54.pdf)
  [2] SAM D5x/E5x Family Silicon Errata and Data Sheet Clarification
    (\\fileserver\Techinfo\Company\Microchip\MCU\ARM\ATSAME54\SAM_D5x_E5x_Family_Silicon_Errata_DS80000748L.pdf)
  [3] SAM E54 Xplained Pro User's Guide
    (\\fileserver\Techinfo\Company\Microchip\MCU\ARM\EvalBoard\SAME54XPlainedPro\70005321A.pdf)
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
#ifndef   FS_NOR_HW_SPIFI_PER_CLK_HZ
  #define FS_NOR_HW_SPIFI_PER_CLK_HZ              120000000         // Clock of QSPI unit.
#endif

#ifndef   FS_NOR_HW_SPIFI_NOR_CLK_HZ
  #define FS_NOR_HW_SPIFI_NOR_CLK_HZ              60000000          // Frequency of the clock supplied to NOR flash device.
#endif

#ifndef   FS_NOR_HW_SPIFI_USE_MEM_MODE
  #define FS_NOR_HW_SPIFI_USE_MEM_MODE            1                 // Enables or disables the reading of NOR flash data via system memory.
                                                                    // Has to be set to 0 for NOR flash devices with a capacity larger than 16 MB
                                                                    // because the system memory window where the contents of the NOR flash device
                                                                    // is mapped is only 16 MB wide.
#endif

/*********************************************************************
*
*       Defines, fixed
*
**********************************************************************
*/

/*********************************************************************
*
*       PORTA
*/
#define PORTA_BASE_ADDR                   0x41008000uL
#define PORTA_DIR                         (*(volatile U32 *)(PORTA_BASE_ADDR + 0x00))    // Data Direction
#define PORTA_DIRCLR                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x04))    // Data Direction Clear
#define PORTA_DIRSET                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x08))    // Data Direction Set
#define PORTA_DIRTGL                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x0C))    // Data Direction Toggle
#define PORTA_OUT                         (*(volatile U32 *)(PORTA_BASE_ADDR + 0x10))    // Data Output Value
#define PORTA_OUTCLR                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x14))    // Data Output Value Clear
#define PORTA_OUTSET                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x18))    // Data Output Value Set
#define PORTA_OUTTGL                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x1C))    // Data Output Value Toggle
#define PORTA_IN                          (*(volatile U32 *)(PORTA_BASE_ADDR + 0x20))    // Data Input Value
#define PORTA_CTRL                        (*(volatile U32 *)(PORTA_BASE_ADDR + 0x24))    // Control
#define PORTA_WRCONFIG                    (*(volatile U32 *)(PORTA_BASE_ADDR + 0x28))    // Write Configuration
#define PORTA_EVCTRL                      (*(volatile U32 *)(PORTA_BASE_ADDR + 0x2C))    // Event Input Control
#define PORTA_PMUX0                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x30))    // Peripheral Multiplexing 0
#define PORTA_PMUX1                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x31))    // Peripheral Multiplexing 1
#define PORTA_PMUX2                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x32))    // Peripheral Multiplexing 2
#define PORTA_PMUX3                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x33))    // Peripheral Multiplexing 3
#define PORTA_PMUX4                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x34))    // Peripheral Multiplexing 4
#define PORTA_PMUX5                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x35))    // Peripheral Multiplexing 5
#define PORTA_PMUX6                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x36))    // Peripheral Multiplexing 6
#define PORTA_PMUX7                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x37))    // Peripheral Multiplexing 7
#define PORTA_PMUX8                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x38))    // Peripheral Multiplexing 8
#define PORTA_PMUX9                       (*(volatile U8  *)(PORTA_BASE_ADDR + 0x39))    // Peripheral Multiplexing 9
#define PORTA_PMUX10                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3A))    // Peripheral Multiplexing 10
#define PORTA_PMUX11                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3B))    // Peripheral Multiplexing 11
#define PORTA_PMUX12                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3C))    // Peripheral Multiplexing 12
#define PORTA_PMUX13                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3D))    // Peripheral Multiplexing 13
#define PORTA_PMUX14                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3E))    // Peripheral Multiplexing 14
#define PORTA_PMUX15                      (*(volatile U8  *)(PORTA_BASE_ADDR + 0x3F))    // Peripheral Multiplexing 15
#define PORTA_PINCFG0                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x40))    // Pin Configuration 0
#define PORTA_PINCFG1                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x41))    // Pin Configuration 1
#define PORTA_PINCFG2                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x42))    // Pin Configuration 2
#define PORTA_PINCFG3                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x43))    // Pin Configuration 3
#define PORTA_PINCFG4                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x44))    // Pin Configuration 4
#define PORTA_PINCFG5                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x45))    // Pin Configuration 5
#define PORTA_PINCFG6                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x46))    // Pin Configuration 6
#define PORTA_PINCFG7                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x47))    // Pin Configuration 7
#define PORTA_PINCFG8                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x48))    // Pin Configuration 8
#define PORTA_PINCFG9                     (*(volatile U8  *)(PORTA_BASE_ADDR + 0x49))    // Pin Configuration 9
#define PORTA_PINCFG10                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4A))    // Pin Configuration 10
#define PORTA_PINCFG11                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4B))    // Pin Configuration 11
#define PORTA_PINCFG12                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4C))    // Pin Configuration 12
#define PORTA_PINCFG13                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4D))    // Pin Configuration 13
#define PORTA_PINCFG14                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4E))    // Pin Configuration 14
#define PORTA_PINCFG15                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x4F))    // Pin Configuration 15
#define PORTA_PINCFG16                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x50))    // Pin Configuration 16
#define PORTA_PINCFG17                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x51))    // Pin Configuration 17
#define PORTA_PINCFG18                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x52))    // Pin Configuration 18
#define PORTA_PINCFG19                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x53))    // Pin Configuration 19
#define PORTA_PINCFG20                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x54))    // Pin Configuration 20
#define PORTA_PINCFG21                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x55))    // Pin Configuration 21
#define PORTA_PINCFG22                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x56))    // Pin Configuration 22
#define PORTA_PINCFG23                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x57))    // Pin Configuration 23
#define PORTA_PINCFG24                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x58))    // Pin Configuration 24
#define PORTA_PINCFG25                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x59))    // Pin Configuration 25
#define PORTA_PINCFG26                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5A))    // Pin Configuration 26
#define PORTA_PINCFG27                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5B))    // Pin Configuration 27
#define PORTA_PINCFG28                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5C))    // Pin Configuration 28
#define PORTA_PINCFG29                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5D))    // Pin Configuration 29
#define PORTA_PINCFG30                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5E))    // Pin Configuration 30
#define PORTA_PINCFG31                    (*(volatile U8  *)(PORTA_BASE_ADDR + 0x5F))    // Pin Configuration 31

/*********************************************************************
*
*       PORTB
*/
#define PORTB_BASE_ADDR                   0x41008080uL
#define PORTB_DIR                         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x00))    // Data Direction
#define PORTB_DIRCLR                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x04))    // Data Direction Clear
#define PORTB_DIRSET                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x08))    // Data Direction Set
#define PORTB_DIRTGL                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x0C))    // Data Direction Toggle
#define PORTB_OUT                         (*(volatile U32 *)(PORTB_BASE_ADDR + 0x10))    // Data Output Value
#define PORTB_OUTCLR                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x14))    // Data Output Value Clear
#define PORTB_OUTSET                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x18))    // Data Output Value Set
#define PORTB_OUTTGL                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x1C))    // Data Output Value Toggle
#define PORTB_IN                          (*(volatile U32 *)(PORTB_BASE_ADDR + 0x20))    // Data Input Value
#define PORTB_CTRL                        (*(volatile U32 *)(PORTB_BASE_ADDR + 0x24))    // Control
#define PORTB_WRCONFIG                    (*(volatile U32 *)(PORTB_BASE_ADDR + 0x28))    // Write Configuration
#define PORTB_EVCTRL                      (*(volatile U32 *)(PORTB_BASE_ADDR + 0x2C))    // Event Input Control
#define PORTB_PMUX0                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x30))    // Peripheral Multiplexing 0
#define PORTB_PMUX1                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x31))    // Peripheral Multiplexing 1
#define PORTB_PMUX2                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x32))    // Peripheral Multiplexing 2
#define PORTB_PMUX3                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x33))    // Peripheral Multiplexing 3
#define PORTB_PMUX4                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x34))    // Peripheral Multiplexing 4
#define PORTB_PMUX5                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x35))    // Peripheral Multiplexing 5
#define PORTB_PMUX6                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x36))    // Peripheral Multiplexing 6
#define PORTB_PMUX7                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x37))    // Peripheral Multiplexing 7
#define PORTB_PMUX8                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x38))    // Peripheral Multiplexing 8
#define PORTB_PMUX9                       (*(volatile U8  *)(PORTB_BASE_ADDR + 0x39))    // Peripheral Multiplexing 9
#define PORTB_PMUX10                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3A))    // Peripheral Multiplexing 10
#define PORTB_PMUX11                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3B))    // Peripheral Multiplexing 11
#define PORTB_PMUX12                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3C))    // Peripheral Multiplexing 12
#define PORTB_PMUX13                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3D))    // Peripheral Multiplexing 13
#define PORTB_PMUX14                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3E))    // Peripheral Multiplexing 14
#define PORTB_PMUX15                      (*(volatile U8  *)(PORTB_BASE_ADDR + 0x3F))    // Peripheral Multiplexing 15
#define PORTB_PINCFG0                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x40))    // Pin Configuration 0
#define PORTB_PINCFG1                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x41))    // Pin Configuration 1
#define PORTB_PINCFG2                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x42))    // Pin Configuration 2
#define PORTB_PINCFG3                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x43))    // Pin Configuration 3
#define PORTB_PINCFG4                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x44))    // Pin Configuration 4
#define PORTB_PINCFG5                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x45))    // Pin Configuration 5
#define PORTB_PINCFG6                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x46))    // Pin Configuration 6
#define PORTB_PINCFG7                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x47))    // Pin Configuration 7
#define PORTB_PINCFG8                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x48))    // Pin Configuration 8
#define PORTB_PINCFG9                     (*(volatile U8  *)(PORTB_BASE_ADDR + 0x49))    // Pin Configuration 9
#define PORTB_PINCFG10                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4A))    // Pin Configuration 10
#define PORTB_PINCFG11                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4B))    // Pin Configuration 11
#define PORTB_PINCFG12                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4C))    // Pin Configuration 12
#define PORTB_PINCFG13                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4D))    // Pin Configuration 13
#define PORTB_PINCFG14                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4E))    // Pin Configuration 14
#define PORTB_PINCFG15                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x4F))    // Pin Configuration 15
#define PORTB_PINCFG16                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x50))    // Pin Configuration 16
#define PORTB_PINCFG17                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x51))    // Pin Configuration 17
#define PORTB_PINCFG18                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x52))    // Pin Configuration 18
#define PORTB_PINCFG19                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x53))    // Pin Configuration 19
#define PORTB_PINCFG20                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x54))    // Pin Configuration 20
#define PORTB_PINCFG21                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x55))    // Pin Configuration 21
#define PORTB_PINCFG22                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x56))    // Pin Configuration 22
#define PORTB_PINCFG23                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x57))    // Pin Configuration 23
#define PORTB_PINCFG24                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x58))    // Pin Configuration 24
#define PORTB_PINCFG25                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x59))    // Pin Configuration 25
#define PORTB_PINCFG26                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5A))    // Pin Configuration 26
#define PORTB_PINCFG27                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5B))    // Pin Configuration 27
#define PORTB_PINCFG28                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5C))    // Pin Configuration 28
#define PORTB_PINCFG29                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5D))    // Pin Configuration 29
#define PORTB_PINCFG30                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5E))    // Pin Configuration 30
#define PORTB_PINCFG31                    (*(volatile U8  *)(PORTB_BASE_ADDR + 0x5F))    // Pin Configuration 31

/*********************************************************************
*
*       QSPI
*/
#define QSPI_BASE_ADDR                    0x42003400uL
#define QSPI_CTRLA                        (*(volatile U32 *)(QSPI_BASE_ADDR + 0x00))     // Control A
#define QSPI_CTRLB                        (*(volatile U32 *)(QSPI_BASE_ADDR + 0x04))     // Control B
#define QSPI_BAUD                         (*(volatile U32 *)(QSPI_BASE_ADDR + 0x08))     // Baud Rate
#define QSPI_RXDATA                       (*(volatile U8  *)(QSPI_BASE_ADDR + 0x0C))     // Receive Data
#define QSPI_TXDATA                       (*(volatile U8  *)(QSPI_BASE_ADDR + 0x10))     // Transmit Data
#define QSPI_INTENCLR                     (*(volatile U32 *)(QSPI_BASE_ADDR + 0x14))     // Interrupt Enable Clear
#define QSPI_INTENSET                     (*(volatile U32 *)(QSPI_BASE_ADDR + 0x18))     // Interrupt Enable Set
#define QSPI_INTFLAG                      (*(volatile U32 *)(QSPI_BASE_ADDR + 0x1C))     // Interrupt Flag Status and Clear
#define QSPI_STATUS                       (*(volatile U32 *)(QSPI_BASE_ADDR + 0x20))     // Status Register
#define QSPI_INSTRADDR                    (*(volatile U32 *)(QSPI_BASE_ADDR + 0x30))     // Instruction Address
#define QSPI_INSTRCTRL                    (*(volatile U32 *)(QSPI_BASE_ADDR + 0x34))     // Instruction Code
#define QSPI_INSTRFRAME                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x38))     // Instruction Frame
#define QSPI_SCRAMBCTRL                   (*(volatile U32 *)(QSPI_BASE_ADDR + 0x40))     // Scrambling Mode
#define QSPI_SCRAMBKEY                    (*(volatile U32 *)(QSPI_BASE_ADDR + 0x44))     // Scrambling Key

/*********************************************************************
*
*       MCLK
*/
#define MCLK_BASE_ADDR                    0x40000800uL
#define MCLK_AHBMASK                      (*(volatile U32 *)(MCLK_BASE_ADDR + 0x10))
#define MCLK_APBBMASK                     (*(volatile U32 *)(MCLK_BASE_ADDR + 0x18))

/*********************************************************************
*
*       PAC
*/
#define PAC_BASE_ADDR                     0x40000000uL
#define PAC_WRCTRL                        (*(volatile U32 *)(PAC_BASE_ADDR + 0x00))
#define PAC_STATUSB                       (*(volatile U32 *)(PAC_BASE_ADDR + 0x38))
#define PAC_STATUSC                       (*(volatile U32 *)(PAC_BASE_ADDR + 0x3C))

/*********************************************************************
*
*       NOR flash port pins
*/
#define NOR_QCS_BIT                       11    // PORTB
#define NOR_QSCK_BIT                      10    // PORTB
#define NOR_QIO0_BIT                      8     // PORTA
#define NOR_QIO1_BIT                      9     // PORTA
#define NOR_QIO2_BIT                      10    // PORTA
#define NOR_QIO3_BIT                      11    // PORTA

/*********************************************************************
*
*       Port pin configuration
*/
#define PINCFG_PMUX_BIT                   0
#define PINCFG_INEN_BIT                   1
#define PINCFG_PULLEN_BIT                 2
#define PINCFG_DRVSTR_BIT                 6
#define PMUX_EVEN_BIT                     0
#define PMUX_EVEN_MASK                    0xFu
#define PMUX_ODD_BIT                      4
#define PMUX_ODD_MASK                     0xFu
#define PMUX_H_FUNC                       0x7u

/*********************************************************************
*
*       First control register
*/
#define CTRLA_SWRST_BIT                   0
#define CTRLA_ENABLE_BIT                  1
#define CTRLA_LASTXFER_BIT                24

/*********************************************************************
*
*       Second control register
*/
#define CTRLB_MODE_BIT                    0
#define CTRLB_CSMODE_BIT                  4
#define CTRLB_CSMODE_SYSTEMATICALLY       0x02uL
#define CTRLB_SEMREG_BIT                  3

/*********************************************************************
*
*       Instruction frame
*/
#define INSTRFRAME_WIDTH_BIT              0
#define INSTRFRAME_WIDTH_SINGLE_BIT_SPI   0x0uL
#define INSTRFRAME_WIDTH_DUAL_OUTPUT      0x1uL
#define INSTRFRAME_WIDTH_QUAD_OUTPUT      0x2uL
#define INSTRFRAME_WIDTH_DUAL_IO          0x3uL
#define INSTRFRAME_WIDTH_QUAD_IO          0x4uL
#define INSTRFRAME_WIDTH_DUAL_CMD         0x5uL
#define INSTRFRAME_WIDTH_QUAD_CMD         0x6uL
#define INSTRFRAME_INSTREN_BIT            4
#define INSTRFRAME_ADDREN_BIT             5
#define INSTRFRAME_DATAEN_BIT             7
#define INSTRFRAME_ADDRLEN_BIT            10
#define INSTRFRAME_TFRTYPE_BIT            12
#define INSTRFRAME_TFRTYPE_READMEMORY     0x01uL
#define INSTRFRAME_TFRTYPE_WRITE          0x02uL
#define INSTRFRAME_TFRTYPE_MASK           0x03uL
#define INSTRFRAME_DUMMYLEN_BIT           16

/*********************************************************************
*
*       Interrupt flags
*/
#define INTFLAG_RXC_BIT                   0
#define INTFLAG_DRE_BIT                   1
#define INTFLAG_TXC_BIT                   2
#define INTFLAG_INSTREND_BIT              10

/*********************************************************************
*
*       Baud rate control
*/
#define BAUD_CPOL_BIT                     0
#define BAUD_CPHA_BIT                     1
#define BAUD_BAUD_BIT                     8
#define BAUD_BAUD_MAX                     0xFFuL

/*********************************************************************
*
*       Write protection
*/
#define INTFLAGB_PORT_BIT                 4
#define INTFLAGC_QSPI_BIT                 13
#define WRCTRL_PERID_BIT                  0
#define WRCTRL_KEY_BIT                    16
#define WRCTRL_KEY_CLEAR                  0x01uL
#define WRCTRL_KEY_SET                    0x02uL
#define WRCTRL_PERID_PORT                 (32 + INTFLAGB_PORT_BIT)
#define WRCTRL_PERID_QSPI                 (64 + INTFLAGC_QSPI_BIT)

/*********************************************************************
*
*       Misc. registers
*/
#define STATUS_ENABLE_BIT                 1
#define STATUS_CCSTATUS_BIT               9
#define INSTRCTRL_INSTR_BIT               0
#define AHBMASK_PAC_BIT                   0
#define AHBMASK_QSPI_BIT                  13
#define AHBMASK_QSPI_2X_BIT               21
#define APBBMASK_PORT_BIT                 4
#define CYCLES_PER_MS                     10000
#define WAIT_TIMEOUT_MS                   100
#define WAIT_TIMEOUT_CYCLES               (CYCLES_PER_MS * WAIT_TIMEOUT_MS)

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CalcBaud
*
*  Function description
*    Calculates the divisor for the clock supplied to NOR flash device.
*/
static U32 _CalcBaud(U32 * pFreq_Hz) {
  U32 Baud;
  U32 Freq_Hz;
  U32 MaxFreq_Hz;

  Baud       = 0;
  MaxFreq_Hz = *pFreq_Hz;
  for (;;) {
    Freq_Hz = FS_NOR_HW_SPIFI_PER_CLK_HZ / (Baud + 1);
    if (Freq_Hz <= MaxFreq_Hz) {
      break;
    }
    ++Baud;
    if (Baud == BAUD_BAUD_MAX) {
      break;
    }
  }
  *pFreq_Hz = Freq_Hz;
  return Baud;
}

/*********************************************************************
*
*       _CalcWidth
*
*  Function description
*    Calculates number of data lines to be used for the data transfer.
*/
static U32 _CalcWidth(U16 BusWidth) {
  unsigned BusWidthCmd;
  unsigned BusWidthAddr;
  unsigned BusWidthData;
  U32      Width;

  Width        = INSTRFRAME_WIDTH_SINGLE_BIT_SPI;
  BusWidthCmd  = FS_BUSWIDTH_GET_CMD(BusWidth);
  BusWidthAddr = FS_BUSWIDTH_GET_ADDR(BusWidth);
  BusWidthData = FS_BUSWIDTH_GET_DATA(BusWidth);
  if (BusWidthCmd == 4) {
    Width = INSTRFRAME_WIDTH_QUAD_CMD;
  } else {
    if (BusWidthCmd == 2) {
      Width = INSTRFRAME_WIDTH_DUAL_CMD;
    } else {
      if (BusWidthAddr == 4) {
        Width = INSTRFRAME_WIDTH_QUAD_IO;
      } else {
        if (BusWidthAddr == 2) {
          Width = INSTRFRAME_WIDTH_DUAL_IO;
        } else {
          if (BusWidthData == 4) {
            Width = INSTRFRAME_WIDTH_QUAD_OUTPUT;
          } else {
            if (BusWidthData == 2) {
              Width = INSTRFRAME_WIDTH_DUAL_OUTPUT;
            }
          }
        }
      }
    }
  }
  return Width;
}

/*********************************************************************
*
*       _CalcDummyLen
*
*  Function description
*    Calculates number of dummy cycles to be sent.
*/
static U32 _CalcDummyLen(unsigned NumBytes, U16 BusWidth) {
  U32      DummyLen;
  unsigned BusWidthAddr;

  DummyLen = 0;
  if (NumBytes != 0) {
    BusWidthAddr = FS_BUSWIDTH_GET_ADDR(BusWidth);
    DummyLen     = NumBytes << 3;        // Assume 8-bits per byte.
    switch (BusWidthAddr) {
    case 2:
      DummyLen >>= 1;
      break;
    case 4:
      DummyLen >>= 2;
      break;
    default:
      break;
    }
  }
  return DummyLen;
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
*    HW layer function. It is called before any other function of the physical layer.
*    It should configure the HW so that the other functions can access the NOR flash.
*
*  Return value
*    Frequency of the SPI clock in Hz.
*/
static int _HW_Init(U8 Unit) {
  U32 Baud;
  U32 Freq_Hz;
  U32 TimeOut;
  int IsWP;

  FS_USE_PARA(Unit);
  //
  // Enable the clock of QSPI controller and of GPIO ports.
  //
  MCLK_AHBMASK  |= 0
                | (1uL << AHBMASK_QSPI_BIT)
                | (1uL << AHBMASK_QSPI_2X_BIT)
                ;
  MCLK_APBBMASK |= 1uL << APBBMASK_PORT_BIT;
  //
  // If required, disable the write protection of GPIO registers.
  //
  IsWP = 0;
  if ((MCLK_AHBMASK & (1uL << AHBMASK_PAC_BIT)) != 0) {
    if ((PAC_STATUSB & (1uL << INTFLAGB_PORT_BIT)) != 0) {
      PAC_WRCTRL = 0
                 | (WRCTRL_KEY_CLEAR  << WRCTRL_KEY_BIT)
                 | (WRCTRL_PERID_PORT << WRCTRL_PERID_BIT)
                 ;
      IsWP = 1;
    }
  }
  //
  // Configure the chip select signal.
  //
  PORTB_PMUX5    &= ~(PMUX_ODD_MASK << PMUX_ODD_BIT);
  PORTB_PMUX5    |=   PMUX_H_FUNC   << PMUX_ODD_BIT;
  PORTB_PINCFG11  = 0
                  | (1u << PINCFG_PMUX_BIT)
                  | (1u << PINCFG_INEN_BIT)
                  | (1u << PINCFG_PULLEN_BIT)
                  | (1u << PINCFG_DRVSTR_BIT)
                  ;
  //
  // Configure the clock signal.
  //
  PORTB_PMUX5    &= ~(PMUX_EVEN_MASK << PMUX_EVEN_BIT);
  PORTB_PMUX5    |=   PMUX_H_FUNC    << PMUX_EVEN_BIT;
  PORTB_PINCFG10 = 0u
                 | (1u << PINCFG_PMUX_BIT)
                 | (1u << PINCFG_INEN_BIT)
                 | (1u << PINCFG_PULLEN_BIT)
                 | (1u << PINCFG_DRVSTR_BIT)
                 ;
  //
  // Configure the 1st data line.
  //
  PORTA_PMUX4    &= ~(PMUX_EVEN_MASK << PMUX_EVEN_BIT);
  PORTA_PMUX4    |=   PMUX_H_FUNC    << PMUX_EVEN_BIT;
  PORTA_PINCFG8  = 0u
                 | (1u << PINCFG_PMUX_BIT)
                 | (1u << PINCFG_INEN_BIT)
                 | (1u << PINCFG_PULLEN_BIT)
                 | (1u << PINCFG_DRVSTR_BIT)
                 ;
  //
  // Configure the 2nd data line.
  //
  PORTA_PMUX4    &= ~(PMUX_ODD_MASK << PMUX_ODD_BIT);
  PORTA_PMUX4    |=   PMUX_H_FUNC   << PMUX_ODD_BIT;
  PORTA_PINCFG9  = 0u
                 | (1u << PINCFG_PMUX_BIT)
                 | (1u << PINCFG_INEN_BIT)
                 | (1u << PINCFG_PULLEN_BIT)
                 | (1u << PINCFG_DRVSTR_BIT)
                 ;
  //
  // Configure the 3rd data line.
  //
  PORTA_PMUX5    &= ~(PMUX_EVEN_MASK << PMUX_EVEN_BIT);
  PORTA_PMUX5    |=   PMUX_H_FUNC    << PMUX_EVEN_BIT;
  PORTA_PINCFG10 = 0u
                 | (1u << PINCFG_PMUX_BIT)
                 | (1u << PINCFG_INEN_BIT)
                 | (1u << PINCFG_PULLEN_BIT)
                 | (1u << PINCFG_DRVSTR_BIT)
                 ;
  //
  // Configure the 4th data line.
  //
  PORTA_PMUX5    &= ~(PMUX_ODD_MASK << PMUX_ODD_BIT);
  PORTA_PMUX5    |=   PMUX_H_FUNC   << PMUX_ODD_BIT;
  PORTA_PINCFG11 = 0u
                 | (1u << PINCFG_PMUX_BIT)
                 | (1u << PINCFG_INEN_BIT)
                 | (1u << PINCFG_PULLEN_BIT)
                 | (1u << PINCFG_DRVSTR_BIT)
                 ;
  //
  // If required, enable the write protection of GPIO registers.
  //
  if (IsWP != 0) {
    PAC_WRCTRL = 0
               | (WRCTRL_KEY_SET    << WRCTRL_KEY_BIT)
               | (WRCTRL_PERID_PORT << WRCTRL_PERID_BIT)
               ;
  }
  //
  // If required, disable the write protection of QSPI registers.
  //
  IsWP = 0;
  if ((MCLK_AHBMASK & (1uL << AHBMASK_PAC_BIT)) != 0) {
    if ((PAC_STATUSC & (1uL << INTFLAGC_QSPI_BIT)) != 0) {
      PAC_WRCTRL = 0
                 | (WRCTRL_KEY_CLEAR  << WRCTRL_KEY_BIT)
                 | (WRCTRL_PERID_QSPI << WRCTRL_PERID_BIT)
                 ;
      IsWP = 1;
    }
  }
  Freq_Hz = FS_NOR_HW_SPIFI_NOR_CLK_HZ;
  Baud    = _CalcBaud(&Freq_Hz);
  //
  // Reset and disable QSPI.
  //
  QSPI_CTRLA |= 1uL << CTRLA_SWRST_BIT;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QSPI_CTRLA & (1uL << CTRLA_SWRST_BIT)) == 0u) {
      break;
    }
    if (--TimeOut == 0) {
      Freq_Hz = 0;                    // Error, could not reset QSPI.
      break;
    }
  }
  //
  // Configure QSPI.
  //
  QSPI_CTRLB = 0uL
             | (1uL << CTRLB_MODE_BIT)
             | (1uL << CTRLB_SEMREG_BIT)
             ;
  QSPI_BAUD  = 0uL
             | (1uL  << BAUD_CPOL_BIT)
             | (1uL  << BAUD_CPHA_BIT)
             | (Baud << BAUD_BAUD_BIT)
             ;
  //
  // Enable QSPI.
  //
  QSPI_CTRLA |= 1uL << CTRLA_ENABLE_BIT;
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QSPI_STATUS & (1uL << STATUS_ENABLE_BIT)) != 0u) {
      break;
    }
    if (--TimeOut == 0) {
      Freq_Hz = 0;                    // Error, could not enable QSPI.
      break;
    }
  }
  //
  // If required, enable the write protection of QSPI registers.
  //
  if (IsWP != 0) {
    PAC_WRCTRL = 0
               | (WRCTRL_KEY_SET    << WRCTRL_KEY_BIT)
               | (WRCTRL_PERID_QSPI << WRCTRL_PERID_BIT)
               ;
  }
  return Freq_Hz;
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
  U32 TimeOut;

  FS_USE_PARA(Unit);
  if ((QSPI_STATUS & (1uL << STATUS_CCSTATUS_BIT)) == 0u) {
    //
    // Indicate the end of data transfer and wait for the commend to be completed.
    //
    QSPI_CTRLA |= 0u
               | (1uL << CTRLA_LASTXFER_BIT)
               | (1uL << CTRLA_ENABLE_BIT)          // Because this bit reads as 0 even when it is set.
               ;
    //
    // Wait for the instruction to end.
    //
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((QSPI_INTFLAG & (1uL << INTFLAG_INSTREND_BIT)) != 0u) {
        QSPI_INTFLAG |= 1uL << INTFLAG_INSTREND_BIT;
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
  }
}

#if FS_NOR_HW_SPIFI_USE_MEM_MODE

/*********************************************************************
*
*       _HW_SetMemMode
*
*  Function description
*    HW layer function. It enables the memory-mapped mode. In this mode
*    the data can be accessed by doing read operations from memory.
*    The HW is responsible to transfer the data via SPI.
*    This function disables the direct access to NOR flash via SPI.
*/
static void _HW_SetMemMode(U8 Unit, U8 ReadCmd, unsigned NumBytesAddr, unsigned NumBytesDummy, U16 BusWidth) {
  U32          Width;
  U32          IsAddr4Byte;
  U32          DummyLen;
  volatile U32 Dummy;

  FS_USE_PARA(Unit);
  IsAddr4Byte = 0;
  if (NumBytesAddr == 4) {
    IsAddr4Byte = 1;
  }
  Width    = _CalcWidth(BusWidth);
  DummyLen = _CalcDummyLen(NumBytesDummy, BusWidth);
  //
  // Configure the QSPI controller to read data on an access
  // to system memory.
  //
  QSPI_INSTRCTRL  = ReadCmd << INSTRCTRL_INSTR_BIT;
  QSPI_INSTRADDR  = 0;
  QSPI_INSTRFRAME = 0u
                  | (1uL                           << INSTRFRAME_ADDREN_BIT)
                  | (IsAddr4Byte                   << INSTRFRAME_ADDRLEN_BIT)
                  | (1uL                           << INSTRFRAME_DATAEN_BIT)
                  | (Width                         << INSTRFRAME_WIDTH_BIT)
                  | (DummyLen                      << INSTRFRAME_DUMMYLEN_BIT)
                  | (INSTRFRAME_TFRTYPE_READMEMORY << INSTRFRAME_TFRTYPE_BIT)
                  | (1uL                           << INSTRFRAME_INSTREN_BIT)
                  ;
  //
  // Synchronize APB and AHB access as per data sheet.
  //
  Dummy = QSPI_INSTRFRAME;
  FS_USE_PARA(Dummy);
}

#endif // FS_NOR_HW_SPIFI_USE_MEM_MODE

/*********************************************************************
*
*       _HW_ExecCmd
*
*  Function description
*    HW layer function. It requests the NOR flash to execute a simple command.
*    The HW has to be in SPI mode.
*/
static void _HW_ExecCmd(U8 Unit, U8 Cmd, U8 BusWidth) {
  U32 Width;
  U32 TimeOut;

  FS_USE_PARA(Unit);
  if (BusWidth == 4u) {
    Width = INSTRFRAME_WIDTH_QUAD_CMD;
  } else {
    if (BusWidth == 2u) {
      Width = INSTRFRAME_WIDTH_DUAL_CMD;
    } else {
      Width = INSTRFRAME_WIDTH_SINGLE_BIT_SPI;
    }
  }
  QSPI_INSTRCTRL  = Cmd << INSTRCTRL_INSTR_BIT;
  QSPI_INSTRFRAME = 0u
                  | (Width << INSTRFRAME_WIDTH_BIT)
                  | (1uL   << INSTRFRAME_INSTREN_BIT)
                  ;
  //
  // Wait for the command to finish.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QSPI_INTFLAG & (1uL << INTFLAG_INSTREND_BIT)) != 0u) {
      QSPI_INTFLAG |= 1uL << INTFLAG_INSTREND_BIT;
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
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
  U32          Width;
  U32          IsAddrEnabled;
  U32          IsAddr4Byte;
  U32          IsDataEnabled;
  U32          DummyLen;
  U32          Addr;
  U32          NumBytes;
  volatile U32 Dummy;
  U32          TimeOut;

  FS_USE_PARA(Unit);
  IsAddrEnabled = 0;
  IsAddr4Byte   = 0;
  Addr          = 0;
  IsDataEnabled = 0;
  DummyLen      = 0;
  Width = _CalcWidth(BusWidth);
  //
  // Prepare the address and the dummy cycles.
  //
  if (NumBytesPara != 0u) {
    if (NumBytesAddr != 0u) {
      IsAddrEnabled = 1;
      NumBytes = NumBytesAddr;
      do {
        Addr <<= 8;
        Addr |= (U32)(*pPara++);
      } while (--NumBytes != 0u);
    }
    if (NumBytesAddr == 4) {
      IsAddr4Byte = 1;
    }
    if (NumBytesPara > NumBytesAddr) {
      NumBytes = NumBytesPara - NumBytesAddr;
      DummyLen = _CalcDummyLen(NumBytes, BusWidth);
    }
  }
  //
  // Check if data has to be sent.
  //
  if (NumBytesData != 0u) {
    IsDataEnabled = 1;
  }
  //
  // Send the command. Note that the QSPI controller is also reading the first byte of data
  // immediately after sending the command.
  //
  QSPI_INSTRCTRL  = Cmd << INSTRCTRL_INSTR_BIT;
  QSPI_INSTRADDR  = Addr;
  QSPI_INSTRFRAME = 0u
                  | (IsAddrEnabled << INSTRFRAME_ADDREN_BIT)
                  | (IsAddr4Byte   << INSTRFRAME_ADDRLEN_BIT)
                  | (IsDataEnabled << INSTRFRAME_DATAEN_BIT)
                  | (Width         << INSTRFRAME_WIDTH_BIT)
                  | (DummyLen      << INSTRFRAME_DUMMYLEN_BIT)
                  | (1uL           << INSTRFRAME_INSTREN_BIT)
                  ;
  //
  // Synchronize APB and AHB access as per data sheet.
  //
  Dummy = QSPI_INSTRFRAME;
  FS_USE_PARA(Dummy);
  //
  // Move data from NOR flash to MCU if required.
  //
  if (NumBytesData != 0u) {
    NumBytes = NumBytesData;
    do {
      //
      // Wait for the data to be received. Note that the QSPI controller reads the next byte when the QSPI_RXDATA is read.
      //
      TimeOut = WAIT_TIMEOUT_CYCLES;
      for (;;) {
        if ((QSPI_INTFLAG & (1uL << INTFLAG_RXC_BIT)) != 0u) {
          break;
        }
        if (--TimeOut == 0) {
          break;
        }
      }
      *pData++ = QSPI_RXDATA;
    } while (--NumBytes != 0u);
    //
    // Wait for the data transfer to complete.
    //
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((QSPI_INTFLAG & (1uL << INTFLAG_TXC_BIT)) != 0u) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
    //
    // Indicate the end of data transfer and wait for the commend to be completed.
    //
    QSPI_CTRLA |= 0u
               | (1uL << CTRLA_LASTXFER_BIT)
               | (1uL << CTRLA_ENABLE_BIT)          // Because this bit reads as 0 even when it is set.
               ;
  }
  //
  // Wait for the instruction to end.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QSPI_INTFLAG & (1uL << INTFLAG_INSTREND_BIT)) != 0u) {
      QSPI_INTFLAG |= 1uL << INTFLAG_INSTREND_BIT;
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
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
  U32          Width;
  U32          IsAddrEnabled;
  U32          IsAddr4Byte;
  U32          IsDataEnabled;
  U32          DummyLen;
  U32          Addr;
  U32          NumBytes;
  volatile U32 Dummy;
  U32          TimeOut;

  FS_USE_PARA(Unit);
  IsAddrEnabled = 0;
  IsAddr4Byte   = 0;
  Addr          = 0;
  IsDataEnabled = 0;
  DummyLen      = 0;
  Width = _CalcWidth(BusWidth);
  if (NumBytesPara != 0u) {
    if (NumBytesAddr != 0u) {
      IsAddrEnabled = 1;
      NumBytes = NumBytesAddr;
      do {
        Addr <<= 8;
        Addr |= (U32)(*pPara++);
      } while (--NumBytes != 0u);
    }
    if (NumBytesAddr == 4) {
      IsAddr4Byte = 1;
    }
    if (NumBytesPara > NumBytesAddr) {
      NumBytes = NumBytesPara - NumBytesAddr;
      DummyLen = _CalcDummyLen(NumBytes, BusWidth);
    }
  }
  if (NumBytesData != 0u) {
    IsDataEnabled = 1;
  }
  //
  // Send the command.
  //
  QSPI_INSTRCTRL  = Cmd << INSTRCTRL_INSTR_BIT;
  QSPI_INSTRADDR  = Addr;
  QSPI_INSTRFRAME = 0u
                  | (IsAddrEnabled            << INSTRFRAME_ADDREN_BIT)
                  | (IsAddr4Byte              << INSTRFRAME_ADDRLEN_BIT)
                  | (IsDataEnabled            << INSTRFRAME_DATAEN_BIT)
                  | (Width                    << INSTRFRAME_WIDTH_BIT)
                  | (DummyLen                 << INSTRFRAME_DUMMYLEN_BIT)
                  | (INSTRFRAME_TFRTYPE_WRITE << INSTRFRAME_TFRTYPE_BIT)
                  | (1uL                      << INSTRFRAME_INSTREN_BIT)
                  ;
  //
  // Synchronize APB and AHB access as per data sheet.
  //
  Dummy = QSPI_INSTRFRAME;
  FS_USE_PARA(Dummy);
  //
  // Move data from MCU to NOR flash device if required.
  //
  if (NumBytesData != 0u) {
    NumBytes = NumBytesData;
    do {
      //
      // Wait for the data to be sent.
      //
      TimeOut = WAIT_TIMEOUT_CYCLES;
      for (;;) {
        if ((QSPI_INTFLAG & (1uL << INTFLAG_DRE_BIT)) != 0u) {
          break;
        }
        if (--TimeOut == 0) {
          break;
        }
      }
      QSPI_TXDATA = *pData++;
    } while (--NumBytes != 0u);
    //
    // Wait for the data transfer to complete.
    //
    TimeOut = WAIT_TIMEOUT_CYCLES;
    for (;;) {
      if ((QSPI_INTFLAG & (1uL << INTFLAG_TXC_BIT)) != 0u) {
        break;
      }
      if (--TimeOut == 0) {
        break;
      }
    }
    //
    // Indicate the end of data transfer and wait for the commend to be completed.
    //
    QSPI_CTRLA |= 0u
               | (1uL << CTRLA_LASTXFER_BIT)
               | (1uL << CTRLA_ENABLE_BIT)          // Because this bit reads as 0 even when it is set.
               ;
  }
  //
  // Wait for the instruction to end.
  //
  TimeOut = WAIT_TIMEOUT_CYCLES;
  for (;;) {
    if ((QSPI_INTFLAG & (1uL << INTFLAG_INSTREND_BIT)) != 0u) {
      QSPI_INTFLAG |= 1uL << INTFLAG_INSTREND_BIT;
      break;
    }
    if (--TimeOut == 0) {
      break;
    }
  }
}

/*********************************************************************
*
*       Global data
*
**********************************************************************
*/
const FS_NOR_HW_TYPE_SPIFI FS_NOR_HW_SPIFI_SAME54P20A_Microchip_SAME54_XPlainedPro = {
  _HW_Init,
  _HW_SetCmdMode,
#if FS_NOR_HW_SPIFI_USE_MEM_MODE
  _HW_SetMemMode,
#else
  NULL,
#endif // FS_NOR_HW_SPIFI_USE_MEM_MODE
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
