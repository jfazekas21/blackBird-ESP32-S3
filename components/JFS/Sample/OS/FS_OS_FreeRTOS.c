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

File    : FS_OS_FreeRTOS.c
Purpose : FreeRTOS OS Layer for the file system.
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS_Int.h"
#include "FS_OS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

/*********************************************************************
*
*       Defines fixed
*
**********************************************************************
*/
#define TICKS_RATE_1MS        1000u

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
static SemaphoreHandle_t    * _paSema;
static SemaphoreHandle_t      _Event;       // Used as binary semaphore. See http://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html
  static U32                 _HighTickBits = 0;
  static U32                 _NumTicksLast = 0;
#if FS_SUPPORT_DEINIT
  static unsigned            _NumLocks;
#endif
#if configSUPPORT_STATIC_ALLOCATION
  static StaticSemaphore_t   _EventBuffer;
  static StaticSemaphore_t * _paSemaBuffer;
#endif

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_X_OS_Lock
*
*  Function description
*    Acquires the specified OS resource.
*
*  Parameters
*    LockIndex    Identifies the OS resource (0-based).
*
*  Additional information
*    This function has to block until it can acquire the OS resource.
*    The OS resource is later released via a call to FS_X_OS_Unlock().
*/
void FS_X_OS_Lock(unsigned LockIndex) {
	SemaphoreHandle_t * pSema;

  pSema = _paSema + LockIndex;
  FS_DEBUG_LOG((FS_MTYPE_OS, "OS: LOCK Index: %d\n", LockIndex));
  (void)xSemaphoreTake(*pSema, portMAX_DELAY);
}

/*********************************************************************
*
*       FS_X_OS_Unlock
*
*  Function description
*    Releases the specified OS resource.
*
*  Parameters
*    LockIndex    Identifies the OS resource (0-based).
*
*  Additional information
*    The OS resource to be released was acquired via a call to FS_X_OS_Lock()
*/
void FS_X_OS_Unlock(unsigned LockIndex) {
	SemaphoreHandle_t * pSema;

  pSema = _paSema + LockIndex;
  FS_DEBUG_LOG((FS_MTYPE_OS, "OS: UNLOCK Index: %d\n", LockIndex));
  (void)xSemaphoreGive(*pSema);
}

/*********************************************************************
*
*       FS_X_OS_Init
*
*  Function description
*    Initializes the OS resources.
*
*  Parameters
*    NumLocks   Number of locks that should be created.
*
*  Additional information
*    This function is called by FS_Init(). It has to create all resources
*    required by the OS to support multi tasking of the file system.
*/
void FS_X_OS_Init(unsigned NumLocks) {
  unsigned           i;
  SemaphoreHandle_t * pSema;
  unsigned           NumBytes;
#if configSUPPORT_STATIC_ALLOCATION
  {
    StaticSemaphore_t * pSemaBuf;
    //
    // Allocate memory for the Mutexes
    //
    NumBytes = NumLocks * sizeof(SemaphoreHandle_t);
    _paSema = SEGGER_PTR2PTR(SemaphoreHandle_t, FS_AllocZeroed((I32)NumBytes));
    //
    // Allocate memory for the static mutex buffers
    //
    NumBytes = NumLocks * sizeof(StaticSemaphore_t);
    _paSemaBuffer = SEGGER_PTR2PTR(StaticSemaphore_t, FS_AllocZeroed((I32)NumBytes));
    pSema =_paSema;
    pSemaBuf = _paSemaBuffer;
    for (i = 0; i < NumLocks; i++) {
      *pSema++ = xSemaphoreCreateMutexStatic(pSemaBuf++);
    }
    _Event = xSemaphoreCreateBinaryStatic(&_EventBuffer);
  }
#else
  NumBytes = NumLocks * sizeof(xSemaphoreHandle);
  _paSema = SEGGER_PTR2PTR(xSemaphoreHandle, FS_AllocZeroed((I32)NumBytes));
  pSema =_paSema;
  for (i = 0; i < NumLocks; i++) {
    *pSema++ = xSemaphoreCreateMutex();
  }
  _Event = xSemaphoreCreateBinary();
#endif
#if FS_SUPPORT_DEINIT
  _NumLocks = NumLocks;
#endif
}

#if FS_SUPPORT_DEINIT

/*********************************************************************
*
*       FS_X_OS_DeInit
*
*  Function description
*    Releases all the OS resources.
*
*  Additional information
*    This function has to release all the resources that have been allocated
*    by FS_X_OS_Init().
*/
void FS_X_OS_DeInit(void) {
  unsigned           i;
  xSemaphoreHandle * pSema;
  unsigned           NumLocks;

  NumLocks = _NumLocks;
  pSema    = _paSema;
  for (i = 0; i < NumLocks; i++) {
    vSemaphoreDelete(*pSema);
    pSema++;
  }
  vSemaphoreDelete(_Event);
  FS_Free(_paSema);
  _paSema   = NULL;
#if configSUPPORT_STATIC_ALLOCATION
  FS_Free(_paSemaBuffer);
  _paSemaBuffer = NULL;
#endif
  _NumLocks = 0;
}

#endif // FS_SUPPORT_DEINIT

/*********************************************************************
*
*       FS_X_OS_GetTime
*
*  Function description
*    Returns the number of milliseconds elapsed since system start.
*/
U32 FS_X_OS_GetTime(void) {
  TickType_t NumTicks;
  U32        ms;
  U32        Quotient;

  NumTicks = xTaskGetTickCount();
  if ((TickType_t)configTICK_RATE_HZ > TICKS_RATE_1MS) {  //lint !e506 !e774 Constant value Boolean [MISRA 2012 Rule 2.1, required] and Boolean within 'if' always evaluates to False [MISRA 2012 Rule 14.3, required].
                                                          // Rationale: configTICK_RATE_HZ is defined in such a way that it cannot be checked using an #if directive.
    Quotient = (U32)configTICK_RATE_HZ / TICKS_RATE_1MS;
    //
    // We have to calculate a value that use the full value range of an U32 before
    // wrapping around, in order to allow calculation of correct time differences.
    // When dividing NumTicks by Quotient the upper bits are lost, so we have to simulate them.
    //
    if (NumTicks < _NumTicksLast) {
      //
      // Wrap around of NumTicks value occurred.
      //
      _HighTickBits += 0xFFFFFFFFuL / Quotient + 1uL;
    }
    _NumTicksLast = NumTicks;
    ms = _HighTickBits + NumTicks / Quotient;
  } else {
    //
    // We use only a single multiplication with a constant to calculate the number of milliseconds
    // with a correct wrap around because calculating it using (Ticks * 1000) / configTICK_RATE_HZ
    // gives accurate results only for small values of NumTicks. For values of NumTicks near 0xFFFFFFFF
    // the calculated number of milliseconds is incorrect.
    //
    ms = NumTicks * (TICKS_RATE_1MS / (U32)configTICK_RATE_HZ);
  }
  return ms;
}

/*********************************************************************
*
*       FS_X_OS_Wait
*
*  Function description
*    Wait for an event to be signaled.
*
*  Parameters
*    TimeOut  Time to wait for the event object. The value is specified in OS ticks.
*
*  Return value:
*    ==0      Event object was signaled within the timeout value
*    !=0      An error or a timeout occurred.
*/
int FS_X_OS_Wait(int TimeOut) {
  int r;

  r = 1;          // Set to indicate a timeout or an error.
  if (xSemaphoreTake(_Event, (TickType_t)TimeOut) == pdTRUE) {
    return 0;     // OK, event signaled.
  }
  return r;
}

/*********************************************************************
*
*       FS_X_OS_Signal
*
*  Function description
*    Indicates that the event occurred.
*/
void FS_X_OS_Signal(void) {
  signed portBASE_TYPE IsHigherPriorityTaskWoken;

  IsHigherPriorityTaskWoken = pdFALSE;
  (void)xSemaphoreGiveFromISR(_Event, &IsHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(IsHigherPriorityTaskWoken);
}

/*********************************************************************
*
*       FS_X_OS_Delay
*
*  Function description
*    Blocks the execution for the specified number of milliseconds.
*
*  Parameters
*    ms       Number of milliseconds to block.
*/
void FS_X_OS_Delay(int ms) {
  TickType_t NumTicks;

  NumTicks = (TickType_t)ms;
  if ((TickType_t)configTICK_RATE_HZ != TICKS_RATE_1MS) { //lint !e506 !e774 Constant value Boolean [MISRA 2012 Rule 2.1, required] and Boolean within 'if' always evaluates to False [MISRA 2012 Rule 14.3, required].
                                                          // Rationale: configTICK_RATE_HZ is defined in such a way that it cannot be checked using an #if directive.
    NumTicks = (NumTicks * (TickType_t)configTICK_RATE_HZ + (TickType_t)(TICKS_RATE_1MS - 1u)) / TICKS_RATE_1MS;
  }
  vTaskDelay(NumTicks);
}

/*************************** End of file ****************************/
