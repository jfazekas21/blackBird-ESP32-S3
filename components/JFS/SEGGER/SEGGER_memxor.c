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
File    : SEGGER_memxor.c
Purpose : Exclusive-or blocks, quickly.
Revision: $Rev: 9275 $
--------  END-OF-HEADER  ---------------------------------------------
*/

/*********************************************************************
*
*       #include Section
*
**********************************************************************
*/

#include "SEGGER.h"

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

/*********************************************************************
*
*       SEGGER_memxor()
*
*  Function description
*    Exclusive-or a block of memory from Src to Dest, i.e.
*    pDest ^= pSrc.
*
*  Parameters
*    pDest[ByteCnt] - Original data and result
*    pSrc[ByteCnt]  - Array to combine with pDest using xor.
*/
void SEGGER_memxor(void *pDest, const void *pSrc, unsigned ByteCnt) {
  U8       *pD;
  const U8 *pS;
  //
  // Manual loop unrolling.
  //
  pD = (U8 *)pDest;
  pS = (const U8 *)pSrc;
  while (ByteCnt >= 16) {
    *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++;
    *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++;
    *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++;
    *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++; *pD++ ^= *pS++;
    ByteCnt -= 16;
  }
  //
  // Last block, if any, handled by looping.
  //
  while (ByteCnt > 0) {
    *pD++ ^= *pS++;
    --ByteCnt;
  }
}

/****** End Of File *************************************************/
