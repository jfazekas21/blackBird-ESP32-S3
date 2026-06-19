  /*
 *  iHub_Actor.h
 *  Created on: Sep 08, 2022
 *  Author: Suraj
 */

#ifndef MAIN_IHUB_ACTOR_H_
#define MAIN_IHUB_ACTOR_H_


#define IHUB_OBJ_QUE_COUNT 		50
#define DeviceID_LEN 			20
#define ConnectionStr_LEN 		200
#define HostName_LEN 		    64

enum{
	IHUB_NOT_INITIALISED = 0,
	IHUB_INITIALISED,
	IHUB_DISCONNECTED,
	IHUB_CONNECTED,
	IHUB_NOT_CONNECTED,
	IHUB_CONNECTION_IN_PROGRESS
};

int iothub_client_device_twin_init(void);
void Print_Heap_size(void);
void IHUB_ConsoleWriteToActor_xface(void *msg);

#endif /* MAIN_IHUB_ACTOR_H_ */
