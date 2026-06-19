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
File        : FS_FAT_VolumeLabel.c
Purpose     : FAT File System Layer for handling the volume label
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS_FAT_Int.h"

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

/*********************************************************************
*
*       _CopyName
*/
static void _CopyName(char * pDest, const U8 * pSrc, unsigned VolumeLabelSize) {
  unsigned   i;
  char     * p;
  unsigned   MaxLen;

  if (VolumeLabelSize != 0u) {
    MaxLen = SEGGER_MIN(11u, VolumeLabelSize);
    FS_MEMCPY(pDest, pSrc, MaxLen);
    --MaxLen;
    p = pDest + MaxLen;
    *p-- = '\0';
    //
    // Remove trailing spaces.
    //
    for (i = MaxLen; i != 0u; i--) {
      if (*p == ' ') {
        *p = '\0';
      } else {
        break;
      }
      p--;
    }
  }
}

/*********************************************************************
*
*       _FindVolumeDirEntry
*
*  Function description
*    Searches for the directory entry which stores the volume name.
*/
static FS_FAT_DENTRY * _FindVolumeDirEntry(FS_VOLUME * pVolume, FS_SB * pSB) {
  FS_FAT_DENTRY * pDirEntry;
  FS_DIR_POS      DirPos;

  FS_FAT_InitDirEntryScan(&pVolume->FSInfo.FATInfo, &DirPos, 0);
  for (;;) {
    pDirEntry = FS_FAT_GetDirEntry(pVolume, pSB, &DirPos);
    if (!pDirEntry) {
      break;
    }
    if (pDirEntry->Data[0] == 0u) {
      pDirEntry = NULL;
      break;  // No more entries. Not found.
    }
    if ((pDirEntry->Data[DIR_ENTRY_OFF_ATTRIBUTES] == FS_FAT_ATTR_VOLUME_ID) &&
        (pDirEntry->Data[0] != 0xE5u)) {    // Attributes does match and not a deleted entry.
      break;
    }
    FS_FAT_IncDirPos(&DirPos);
  }
  return pDirEntry;
}

/*********************************************************************
*
*       _IsValidChar
*
*  Function description
*    Checks if a character is allowed in the name of a volume.
*/
static int _IsValidChar(char c) {
  int r;

  switch (c) {
  case '"':
  case '&':
  case '*':
  case '+':
  case '-':
  case ',':
  case '.':
  case '/':
  case ':':
  case ';':
  case '<':
  case '=':
  case '>':
  case '?':
  case '[':
  case ']':
  case '\\':
    r = 0;        // Invalid character
    break;
  default:
    r = 1;        // Permitted character
    break;
  }
  return r;
}

/*********************************************************************
*
*       _MakeName
*/
static void _MakeName(FS_83NAME * pVolLabel, const char * pVolumeLabel) {
  U8       * p;
  unsigned   MaxLen;
  unsigned   i;

  MaxLen = (unsigned)FS_STRLEN(pVolumeLabel);
  MaxLen = SEGGER_MIN(11u, MaxLen);
  FS_MEMSET(&pVolLabel->ac[0], (int)' ', sizeof(pVolLabel->ac));
  p = &pVolLabel->ac[0];
  for (i = 0; i < MaxLen; i++) {
    char c;

    c = *pVolumeLabel++;
    if (_IsValidChar(c) == 0) {
      *p++ = (U8)'_';
    } else {
      *p++ = (U8)FS_TOUPPER((int)c);
    }
  }
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_FAT_GetVolumeLabel
*
*  Function description
*    Retrieves the label of a FAT volume, if it exists.
*
*  Return value
*    ==0      O.K., volume label read.
*    !=0      Error code indicating the failure reason.
*/
int FS_FAT_GetVolumeLabel(FS_VOLUME * pVolume, char * pVolumeLabel, unsigned VolumeLabelSize) {
  FS_FAT_DENTRY * pDirEntry;
  FS_SB           sb;
  int             r;

  r = FS_ERRCODE_OK;        // Set to indicate success.
  (void)FS__SB_Create(&sb, &pVolume->Partition);
  //
  // Find the volume label entry.
  //
  pDirEntry = _FindVolumeDirEntry(pVolume,  &sb);
  if (pDirEntry != NULL) {
    //
    // volume label found, copy the name.
    //
    _CopyName(pVolumeLabel, pDirEntry->Data, VolumeLabelSize);
  } else {
    //
    // No volume label available.
    //
    *pVolumeLabel = '\0';
    r = FS_ERRCODE_FILE_DIR_NOT_FOUND;        // Error, volume label not found.
  }
  FS__SB_Delete(&sb);
  return r;
}

/*********************************************************************
*
*       FS_FAT_SetVolumeLabel
*
*  Function description
*    Sets a label of a FAT volume.
*
*  Return value
*    ==0      O.K., volume label modified
*    !=0      Error code indicating the failure reason
*/
int FS_FAT_SetVolumeLabel(FS_VOLUME * pVolume, const char * pVolumeLabel) {
  FS_FAT_DENTRY * pDirEntry;
  FS_SB           sb;
  int             r;

  r = FS_ERRCODE_OK;      // Set to indicate success.
  (void)FS__SB_Create(&sb, &pVolume->Partition);
  //
  // Find the volume label entry.
  //
  pDirEntry = _FindVolumeDirEntry(pVolume,  &sb);
  //
  // Create/delete the volume label.
  //
  if (pVolumeLabel != NULL) {
    FS_83NAME VolLabel;
    U32       TimeDate;

    TimeDate = FS__GetTimeDate();
    _MakeName(&VolLabel, pVolumeLabel);
    if (pDirEntry == NULL) {
      pDirEntry = FS_FAT_FindEmptyDirEntry(pVolume, &sb, 0);
    }
    if (pDirEntry != NULL) {
      FS_FAT_WriteDirEntry83(pDirEntry, &VolLabel, 0, FS_FAT_ATTR_VOLUME_ID, 0, TimeDate & 0xFFFFu, TimeDate >> 16, 0);
    } else {
      r = FS_ERRCODE_VOLUME_FULL;
    }
  } else {
    if (pDirEntry != NULL) {
      //
      // Delete this volume label entry
      //
      pDirEntry->Data[0] = 0xE5;
    } else {
      r = FS_ERRCODE_FILE_DIR_NOT_FOUND;      // Error, volume label not found.
    }
  }
  //
  // Mark the volume as dirty.
  //
  FS_FAT_UpdateDirtyFlagIfRequired(pVolume, 1);
  //
  // Update the volume label on the storage.
  //
  FS__SB_MarkDirty(&sb);
  FS__SB_Delete(&sb);
  if (r == 0) {
    r = FS__SB_GetError(&sb);
  }
  return r;
}

/*************************** End of file ****************************/
