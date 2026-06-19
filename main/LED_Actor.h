/*
 * LED_Actor.h
 *
 *  Created on: Apr 29, 2022
 *      Author: shyam
 */

#ifndef MAIN_LED_ACTOR_H_
#define MAIN_LED_ACTOR_H_

#include "actor.h"

#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE

#define CPU_LED		GPIO_NUM_0
#define FB1			GPIO_NUM_2
#define FB2			GPIO_NUM_3
#define CLCK_SYNC	GPIO_NUM_21
#define BLE_STAT	GPIO_NUM_8
#define FUSE		GPIO_NUM_0
#define LEDC_CHANNEL_RED          	LEDC_CHANNEL_0
#define LEDC_CHANNEL_GREEN        	LEDC_CHANNEL_1
#define LEDC_CHANNEL_BLUE       	LEDC_CHANNEL_2
#define LEDC_CHANNEL_BLE        	LEDC_CHANNEL_3
#define LEDC_CHANNEL_FUSE        	LEDC_CHANNEL_4
#define LEDC_DUTY_RES           	LEDC_TIMER_10_BIT // Set duty resolution to 11 bits  //old 13 bits
//#define LEDC_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          	(5000) // Frequency in Hertz. Set frequency at 5 kHz

typedef enum
{
	BLE_LED_DISABLED =0,
	BLE_LED_ADVERT_NOT_CONNECTED = 1,
	BLE_LED_CONNECTED = 2
}BLE_Status;

void LED_ConsoleWriteToActor_xface(void *msg);

#endif /* MAIN_LED_ACTOR_H_ */
