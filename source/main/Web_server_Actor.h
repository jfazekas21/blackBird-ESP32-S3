/*
 * Web_server_Actor.h
 *
 *  Created on: 11-Mar-2024
 *      Author: Acer
 */

#ifndef MAIN_WEB_SERVER_ACTOR_H_
#define MAIN_WEB_SERVER_ACTOR_H_

#include "actor.h"
esp_err_t start_file_server(const char *base_path, AMessage_st* s_Message_Rx);
void WEB_SERVER_ConsoleWriteToActor_xface(void *msg);



#endif /* MAIN_WEB_SERVER_ACTOR_H_ */
