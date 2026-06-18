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
File        : FS_FAT_DiskInfo.c
Purpose     : FAT File System Layer for handling disk information
-------------------------- END-OF-HEADER -----------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/
#include "FS_FAT.h"
#include "FS_FAT_Int.h"

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       FS_FAT_GetDiskInfo
*
*  Function description
*    Get information about used/unused clusters.
*
*  Parameters
*    pVolume    Volume instance.
*    pDiskInfo  Receives the read information.
*    Flags      Bit-mask indicating what information should be returned.
*
*  Return value
*    ==0        Information is stored in pDiskInfo.
*    !=0        Error code indicating the failure reason.
*/
int FS_FAT_GetDiskInfo(FS_VOLUME * pVolume, FS_DISK_INFO * pDiskInfo, int Flags) {
  FS_FAT_INFO * pFATInfo;
  U32           iCluster;
  U32           LastCluster;
  U32           NumFreeClusters;
  FS_SB         sb;
  U8            IsDirty;

  pFATInfo = &pVolume->FSInfo.FATInfo;
  if (pDiskInfo == NULL) {
    return FS_ERRCODE_INVALID_PARA;         // Error, no pointer to a FS_DISK_INFO structure.
  }
  (void)FS__SB_Create(&sb, &pVolume->Partition);
  if (((unsigned)Flags & (unsigned)FS_DISKINFO_FLAG_FREE_SPACE) != 0u) {
    LastCluster = pFATInfo->NumClusters + 1u;
    if ((pFATInfo->NumFreeClusters != NUM_FREE_CLUSTERS_INVALID) && (pFATInfo->NumFreeClusters <= pFATInfo->NumClusters)) {
      NumFreeClusters = pFATInfo->NumFreeClusters;
    } else {
      FS_ENABLE_READ_AHEAD(pVolume);
      //
      // Start to count the empty clusters
      //
      NumFreeClusters = 0;
      for (iCluster = FAT_FIRST_CLUSTER; iCluster <= LastCluster; iCluster++) {
        if (FS_FAT_ReadFATEntry(pVolume, &sb, iCluster) == 0u) {
          NumFreeClusters++;
        }
        if (FS__SB_GetError(&sb) != 0) {
          FS_MEMSET(pDiskInfo, 0, sizeof(FS_DISK_INFO));
          FS__SB_Delete(&sb);
          FS_DISABLE_READ_AHEAD(pVolume);
          return FS_ERRCODE_READ_FAILURE;             // Error, failed to read FAT entry.
        }
      }
      pFATInfo->NumFreeClusters = NumFreeClusters;    // Update FATInfo.
      FS_DISABLE_READ_AHEAD(pVolume);
#if FS_FAT_USE_FSINFO_SECTOR
      {
        FAT_FSINFO_SECTOR * pFSInfoSector;

        pFSInfoSector = &pFATInfo->FSInfoSector;
        if (   (FAT_UseFSInfoSector != 0u)
            && (pFSInfoSector->IsPresent != 0u)
            && (pFSInfoSector->IsUpdateRequired == 0u)) {
          pFSInfoSector->IsUpdateRequired = 1;        // Request that the FSInfo sector is updated either at unmount or at synchronization.
        }
      }
#endif // FS_FAT_USE_FSINFO_SECTOR
    }
#if FS_FAT_SUPPORT_FREE_CLUSTER_CACHE
    {
      //
      // Take into account the clusters allocated from the free cluster cache.
      //
      U32 NumClustersInUse;

      NumClustersInUse = pFATInfo->FreeClusterCache.NumClustersInUse;
      if (NumClustersInUse != 0u) {
        if (NumFreeClusters < NumClustersInUse) {
          NumFreeClusters  = 0;
        } else {
          NumFreeClusters -= NumClustersInUse;
        }
      }
    }
#endif // FS_FAT_SUPPORT_FREE_CLUSTER_CACHE
  } else {
    NumFreeClusters = 0;
  }
#if FS_FAT_UPDATE_DIRTY_FLAG
  IsDirty = pFATInfo->IsDirty;
#else
  IsDirty = 0;
#endif
  pDiskInfo->NumTotalClusters  = pFATInfo->NumClusters;
  pDiskInfo->NumFreeClusters   = NumFreeClusters;
  pDiskInfo->SectorsPerCluster = pFATInfo->SectorsPerCluster;
  pDiskInfo->BytesPerSector    = pFATInfo->BytesPerSector;
  pDiskInfo->NumRootDirEntries = (pFATInfo->FATType == FS_FAT_TYPE_FAT32) ? 0xFFFFu : pFATInfo->RootEntCnt;
  pDiskInfo->FSType            = pFATInfo->FATType;
  pDiskInfo->IsDirty           = IsDirty;
  FS__SB_Delete(&sb);
  return FS_ERRCODE_OK;
}

/*************************** End of file ****************************/
