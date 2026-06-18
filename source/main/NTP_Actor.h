/*
 * NTP_Actor.h
 *
 *  Created on: 21-Jun-2023
 *      Author: Ashwini Mortale
 */

#ifndef MAIN_NTP_ACTOR_H_
#define MAIN_NTP_ACTOR_H_

#ifdef __cplusplus
extern "C" {
#endif


//#define MAX_OUT_SIZE	256
#define OBJ_QUE_COUNT	10


enum {
	RTC_NEVER_SET,
	RTC_DEVICE_ANNOUNCE,
	RTC_IMMEDIATE_MODE,
	RTC_SMOOTH_MODE,
	RTC_SET_POWER_ON,
};

void NTP_ConsolWriteToActor_xface(void *msg);
uint64_t get_current_rtc_time_ms(uint8_t setval);


#ifdef __cplusplus
}
#endif

#endif /* MAIN_NTP_ACTOR_H_ */
