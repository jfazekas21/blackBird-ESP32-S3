/*
 * ACTOR_SYSTEM.h
 *
 *  Created on: 18-Mar-2024
 *      Author: Sanjeev Kumar Gupta
 */

#ifndef MAIN_SYSTEM_ACTOR_H_
#define MAIN_SYSTEM_ACTOR_H_

// Define constants for better readability and maintainability
#define BUFFER_SIZE 4096
#define SSID_SIZE 50
#define PASS_SIZE 50
#define KEY_VAL_SIZE 50
#define CONNECT_STATUS_SIZE 50
#define MESSAGE_SIZE 100
#define WAIT_30_SECONDS 30000 / portTICK_PERIOD_MS
#define WAIT_10_MINUTES 600000 / portTICK_PERIOD_MS

// Define the states
typedef enum {
	MONSRV_STATE_IDLE,
	MONSRV_STATE_CHECK_IHUB_STATUS,
	MONSRV_STATE_CHECK_WIFI_ETH_ENABLED,
	MONSRV_STATE_CHECK_WIFI_ETH_ENABLED_STATUS,
	MONSRV_STATE_CHECK_WIFI_APINFO,
	MONSRV_STATE_CHECK_RESCUE_SSID,
	MONSRV_STATE_DISCONNECT_WIFI,
	MONSRV_STATE_NETWORK_SCAN
} MonSrvConnState;

void SYSTEM_ConsoleWriteToActor_xface(void *msg); // An interface function for the console to send msg to the actor

#endif /* MAIN_SYSTEM_ACTOR_H_ */
