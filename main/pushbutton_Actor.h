/*
 * Pushbutton_Actor.h
 *
 *  Created on: 16-Jun-2023
 *      Author: Ashwini Mortale
 */

#ifndef MAIN_PUSHBUTTON_ACTOR_H_
#define MAIN_PUSHBUTTON_ACTOR_H_

//#define OBJ_QUE_COUNT	10									// No of objects for the messages to this Actor.
//#define MAX_OUT_SIZE	64

//int uart_PB_actor_methods(int argc, char **argv);

void button_main();
void debounce();
void BT1_pressed_check();
void BT2_pressed_check();
void PUSHBUTTON_ConsoleWriteToActor_xface(void *msg);

#endif /* MAIN_PUSHBUTTON_ACTOR_H_ */
