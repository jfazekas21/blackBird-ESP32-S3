/*
 * wifi_actor.h
 *
 *  Created on: May 12, 2022
 *      Author: shyam
 */

#ifndef _WIFI_ACTOR_H_
#define _WIFI_ACTOR_H_


#define SSID_LEN 32
#define PASS_LEN 32
#define MAC_LEN  18

#define DEFAULT_SCAN_LIST_SIZE 	25  //CONFIG_EXAMPLE_SCAN_LIST_SIZE
#define WIFI_CONNECTED 			true
#define DEFAULT_TIMEOUT_MS 		10000

//enum {
//	E_wifi_ZERO_STATE = 0,
//	E_wifi_NOT_CONNECTED,
//	E_wifi_CONNECTED_LOW,
//	E_wifi_CONNECTED_HIGH = 3,
//	E_wifi_CONNECTED_AP,
////	E_SERVER_NOT_CONNECTED,
////	E_SERVER_CONNECTED
//};

void wifi_ConsoleWriteToActor_xface(void *msg); // An interface function for the console to send msg to the actor




#endif /* MAIN_WIFI_ACTOR_H_ */
