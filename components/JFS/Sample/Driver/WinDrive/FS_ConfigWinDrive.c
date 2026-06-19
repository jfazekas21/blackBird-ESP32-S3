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

File    : FS_ConfigWinDrive.c
Purpose : Configuration file for Win driver.
*/

/*********************************************************************
*
*       #include section
*
**********************************************************************
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include "FS.h"
#include "FS_Debug.h"

/*********************************************************************
*
*      Defines, configurable
*
**********************************************************************
*/
#ifndef   FS_ALLOC_SIZE
  #define FS_ALLOC_SIZE       0x1000000
#endif

/*********************************************************************
*
*      Defines, fixed
*
**********************************************************************
*/
#define MEM_UNITS (FS_ALLOC_SIZE + sizeof(U32) - 1) / sizeof(U32)

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static U32   _aMemBlock[MEM_UNITS];           // Memory pool used for semi-dynamic allocation in FS_X_Alloc().

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_X_Panic
*
*  Function description
*    Referred in debug builds of the file system only and
*    called only in case of fatal, unrecoverable errors.
*/
void FS_X_Panic(int ErrorCode) {
  char ac[255];

  sprintf(ac, "The following FS_PANIC occurred: %d", ErrorCode);
  MessageBox(NULL, ac, "FS_X_Panic", MB_ICONERROR | MB_OK);
  while (1);
}

/*********************************************************************
*
*       FS_X_AddDevices
*
*  Function description
*    This function is called by the FS during FS_Init().
*    It is supposed to add all devices, using primarily FS_AddDevice().
*
*  Notes
*    (1) Other API functions may NOT be called, since this function is called
*        during initialization. The devices are not yet ready at this point.
*/
void FS_X_AddDevices(void) {
  FS_AssignMemory(&_aMemBlock[0], sizeof(_aMemBlock));
  //
  // Add and configure the driver.
  //
  FS_AddDevice(&FS_WINDRIVE_Driver);
  FS_WINDRIVE_Configure(0, NULL);
#if FS_SUPPORT_FILE_BUFFER
  //
  // Enable the file buffer to increase the performance when reading/writing a small number of bytes.
  //
  FS_ConfigFileBufferDefault(512, FS_FILE_BUFFER_WRITE);
#endif
}

/*********************************************************************
*
*       FS_X_GetTimeDate
*
*  Function description
*    Current time and date in a format suitable for the file system.
*
*  Additional information
*    Bit 0-4:   2-second count (0-29)
*    Bit 5-10:  Minutes (0-59)
*    Bit 11-15: Hours (0-23)
*    Bit 16-20: Day of month (1-31)
*    Bit 21-24: Month of year (1-12)
*    Bit 25-31: Count of years from 1980 (0-127)
*/
U32 FS_X_GetTimeDate(void) {
  U16         rDate;
  U16         rTime;
  time_t      Time;
  struct tm * pLocalTime;

  time(&Time);
  pLocalTime = localtime(&Time);
  rDate      = (U16) (pLocalTime->tm_mday);
  rDate     |= (U16)((pLocalTime->tm_mon  +  1) << 5);
  rDate     |= (U16)((pLocalTime->tm_year - 80) << 9);
  rTime      = (U16)(pLocalTime->tm_sec   /  2);
  rTime     |= (U16)(pLocalTime->tm_min  <<  5);
  rTime     |= (U16)(pLocalTime->tm_hour << 11);
  return rDate << 16 | rTime;
}

/*********************************************************************
*
*      FS_X_Log
*
*  Notes
*    Logging is used in higher debug levels only. The typical target
*    build does not use logging and does therefore not require any of
*    the logging routines below. For a release build without logging
*    the routines below may be eliminated to save some space.
*    (If the linker is not function aware and eliminates unreferenced
*    functions automatically)
*/
void FS_X_Log(const char *s) {
  printf(s);
}

/*********************************************************************
*
*      FS_X_Warn
*/
void FS_X_Warn(const char *s) {
  printf("FS warning: %s", s);
}

/*********************************************************************
*
*      FS_X_ErrorOut
*/
void FS_X_ErrorOut(const char *s) {
  MessageBox(NULL, s, "FS ErrorOut", MB_ICONERROR | MB_OK);
}

/*************************** End of file ****************************/
