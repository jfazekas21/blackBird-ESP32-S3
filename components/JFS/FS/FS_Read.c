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
----------------------------------------------------------------------
File        : FS_Read.c
Purpose     : Implementation of FS_Read
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include <stdio.h>
#include "FS_Int.h"

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS__Read
*
*  Function description
*    Internal version of FS_Read.
*    Reads data from a file.
*
*  Parameters
*    pFile        Pointer to an opened file handle.
*    pData        Pointer to a buffer to receive the data.
*    NumBytes     Number of bytes to be read.
*
*  Return value
*    Number of bytes read.
*/
U32 FS__Read(FS_FILE * pFile, void * pData, U32 NumBytes) {
  U8            InUse;
  U32           NumBytesRead;
  FS_FILE_OBJ * pFileObj;

  if (NumBytes == 0u) {
    return 0;                 // OK, nothing to read.
  }
  if (pFile == NULL) {
    return 0;                 // Error, no pointer to a FS_FILE structure.
  }
  NumBytesRead = 0;
  //
  // Load file information.
  //
  FS_LOCK_SYS();
  InUse    = pFile->InUse;
  pFileObj = pFile->pFileObj;
  FS_UNLOCK_SYS();
  if ((InUse == 0u) || (pFileObj == NULL)) {
    FS_DEBUG_ERROROUT((FS_MTYPE_API, "FS__Read: File handle closed by application."));
    return 0;
  }
  //
  // Lock driver before performing operation.
  //
  FS_LOCK_DRIVER(&pFileObj->pVolume->Partition.Device);
  //
  // Multi-tasking environments with per-driver-locking:
  // Make sure that relevant file information has not changed (an other task may have closed the file, unmounted the volume etc.)
  // If it has, no action is performed.
  //
#if FS_OS_LOCK_PER_DRIVER
  FS_LOCK_SYS();
  if (pFileObj != pFile->pFileObj) {
    InUse = 0;
  }
  if (pFile->InUse == 0u) {
    InUse = 0;
  }
  FS_UNLOCK_SYS();
  if (InUse == 0u) {                                  // Let's make sure the file is still valid.
    FS_DEBUG_ERROROUT((FS_MTYPE_API, "FS__Read: File handle closed by application."));
  } else
#endif
  //
  // All checks and locking operations completed. Call the File system (FAT/EFS) layer.
  //
  {
    if ((pFile->AccessFlags & FS_FILE_ACCESS_FLAG_R) == 0u) {
      pFile->Error = FS_ERRCODE_WRITE_ONLY_FILE;      // File open mode does not allow read ops.
    } else {
#if FS_SUPPORT_FILE_BUFFER
      int r;

      //
      // Clear the buffers of the other file handles which access the same file.
      //
      r = FS__FB_Sync(pFile);
      if (r == 0) {
        r = FS__FB_Read(pFile, pData, NumBytes);
      }
      if (r < 0) {
        if (pFile->Error == 0) {
          pFile->Error = (I16)r;                      // Error, could not read or write data.
        }
      } else {
        NumBytesRead  = (U32)r;
        NumBytes     -= NumBytesRead;
        if (NumBytes != 0u) {
          NumBytesRead += FS_FILE_READ(pFile, pData, NumBytes);
        }
      }
#else
      NumBytesRead = FS_FILE_READ(pFile, pData, NumBytes);
#endif // FS_SUPPORT_FILE_BUFFER
    }
  }
  FS_UNLOCK_DRIVER(&pFileObj->pVolume->Partition.Device);
  return NumBytesRead;
}

/*********************************************************************
*
*       FS_Read
*
*  Function description
*    Reads data from a file.
*
*  Parameters
*    pFile        Handle to an opened file. It cannot be NULL.
*    pData        Buffer to receive the read data.
*    NumBytes     Number of bytes to be read.
*
*  Return value
*    Number of bytes read.
*
*  Additional information
*    The file has to be opened with read permissions.
*    For more information about open modes refer to FS_FOpen().
*
*    The application has to check for possible errors using FS_FError()
*    if the number of bytes actually read is different than the number
*    of bytes requested to be read by the application.
*
*    The data is read from the current position in the file that is
*    indicated by the file pointer. FS_Read() moves the file pointer
*    forward by the number of bytes successfully read.
*/
U32 FS_Read(FS_FILE * pFile, void * pData, U32 NumBytes) {
  U32 NumBytesRead;

  FS_LOCK();
  FS_PROFILE_CALL_U32x3(FS_EVTID_READ, SEGGER_PTR2ADDR(pFile), SEGGER_PTR2ADDR(pData), NumBytes);
  NumBytesRead = FS__Read(pFile, pData, NumBytes);
  FS_PROFILE_END_CALL_U32(FS_EVTID_READ, NumBytesRead);
  FS_UNLOCK();
  return NumBytesRead;
}

/*********************************************************************
*
*       FS_FRead
*
*  Function description
*    Reads data from file.
*
*  Parameters
*    pData      Buffer that receives the data read from file.
*    ItemSize   Size of one item to be read from file (in bytes).
*    NumItems   Number of items to be read from the file.
*    pFile      Handle to opened file. It cannot be NULL.
*
*  Return value
*    Number of items read.
*
*  Additional information
*    The file has to be opened with read permissions.
*    For more information about open modes refer to FS_FOpen().
*
*    The application has to check for possible errors using FS_FError()
*    if the number of items actually read is different than the number
*    of items requested to be read by the application.
*
*    The data is read from the current position in the file that is
*    indicated by the file pointer. FS_FRead() moves the file pointer
*    forward by the number of bytes successfully read.
*/
U32 FS_FRead(void * pData, U32 ItemSize, U32 NumItems, FS_FILE * pFile) {
  U32 NumBytesRead;
  U32 NumBytes;

  FS_LOCK();
  if (ItemSize == 0u)  {
    FS_UNLOCK();
    return 0;             // Return here to avoid dividing by zero at the end of the function.
  }
  NumBytes = NumItems * ItemSize;
  NumBytesRead = FS__Read(pFile, pData, NumBytes);
  FS_UNLOCK();
  return (NumBytesRead / ItemSize);
}

/*************************** End of file ****************************/
