/*
 * File_Actor.h
 *
 *  Created on: 01-Sep-2022
 *      Author: Amruta Dixit
 */

#ifndef MAIN_SPIFFS_ACTOR_H_
#define MAIN_SPIFFS_ACTOR_H_

void SPIFFS_StartupGateReset(void);
BaseType_t SPIFFS_WaitForStartupReady(TickType_t timeout_ticks);
void SPIFFS_ConsoleWriteToActor_xface(void *msg);

#endif /* MAIN_SPIFFS_ACTOR_H_ */
