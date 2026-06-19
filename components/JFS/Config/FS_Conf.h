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

File    : FS_Conf.h
Purpose : Simple configuration for file system
*/

#ifndef FS_CONF_H
#define FS_CONF_H     // Avoid multiple inclusion

//#define FS_DEBUG_LEVEL      5     // 0: Smallest code, 5: Full debug. See chapter 10 "Debugging" of the emFile manual.
//#define FS_OS_LOCKING       1     // 0: No locking, 1: API locking, 2: Driver locking. See chapter 9 "OS integration" of the emFile manual.
                                  // The application has to provide an OS layer. Sample OS layers are provided in the
                                  // "Sample\FS\OS" folder of the emFile shipment.
/* Optimization Type */
/* Define the optimization type based on your priority: speed, memory, or balanced */
#define FS_OPTIMIZATION_TYPE FS_OPTIMIZATION_TYPE_MAX_SPEED   // Optimized for maximum speed

/* Debugging */
/* Set the appropriate debug level based on whether you're in development or production */
#define FS_DEBUG_LEVEL FS_DEBUG_LEVEL_NOCHECK    // Disable debug checks for better performance in production

/* OS Locking */
/* Enables locking mechanisms if multitasking is used with FreeRTOS */
#define FS_OS_LOCKING FS_OS_LOCKING_API    // Enable API-level locking to prevent concurrent access issues

/* Cache Support */
/* Enable caching to improve read/write speed */
#define FS_SUPPORT_CACHE 1   // Enable sector caching for improved performance

/* Support for Sector Buffer Cache */
/* Improves performance by allowing reuse of sector data between API calls */
#define FS_SUPPORT_SECTOR_BUFFER_CACHE 1   // Enable buffer caching

/* Wear Leveling for NOR Flash */
/* This ensures even distribution of write/erase cycles to prolong NOR flash life */
#define FS_NOR_MAX_ERASE_CNT_DIFF 5000    // Ensure balanced wear leveling across the flash

/* File System Support */
/* Enable the FAT file system for use */
#define FS_SUPPORT_FAT 1     // Support FAT file system

/* Journal Support */
/* Enables journaling to protect against file system corruption in case of power failure */
#define FS_SUPPORT_JOURNAL 1    // Enable journaling to ensure fail-safe operations

/* Path Length */
/* Maximum path length for file names in the file system */
#define FS_MAX_PATH 256    // Supports up to 256 characters in a file path

/* NOR Flash Specific Parameters */
/* Verification ensures that write/erase operations are successful */
#define FS_NOR_VERIFY_WRITE 1   // Verify write operations for data integrity
#define FS_NOR_VERIFY_ERASE 1   // Verify erase operations to ensure sectors are correctly erased
#define FS_NOR_SUPPORT_FAIL_SAFE_ERASE 1   // Ensures that erase operations are performed safely
#define FS_VERIFY_WRITE 1   // Verify all write operations

/* NOR Flash Page Size */
/* Specifies the maximum number of bytes that can be written to NOR flash in a single operation */
#define FS_NOR_BYTES_PER_PAGE 256   // Standard page size for most NOR flash devices
#endif                // Avoid multiple inclusion

/*************************** End of file ****************************/
