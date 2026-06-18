/*
 * UDP_Actor.h
 *
 *  Created on: 03-Nov-2023
 *      Author: Sanjeev Gupta
 */

#ifndef MAIN_UDP_ACTOR_H_
#define MAIN_UDP_ACTOR_H_

#define PORT 3333

enum{
	UDP_INITIALISED = 0,
	UDP_DISCONNECTED,
	UDP_CONNECTED,
};

void UDP_ConsoleWriteToActor_xface(void *msg);


#endif /* MAIN_UDP_ACTOR_H_ */
