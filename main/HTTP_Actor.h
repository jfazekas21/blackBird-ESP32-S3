/*
 * HTTP_Actor.h
 *
 *  Created on: Jun 2, 2022
 *      Author: shyam
 */

#ifndef _HTTP_ACTOR_H_
#define _HTTP_ACTOR_H_

#define MAX_HTTP_RECV_BUFFER 		8192 //4096 //2048  //1024
enum {
	E_SERVER_NOT_CONNECTED = 0,
	E_SERVER_CONNECTED = 1
};
void http_ConsoleWriteToActor_xface(void *msg); // An interface function for the console to send msg to the actor

#endif /* MAIN_HTTP_ACTOR_H_ */
