/*
 * B394_DigitalInput.h
 * B394-only 8-DI (DI0..DI7) actor: debounce/combine window, D2C Input Event,
 * PROPERTIES for DEBOUNCE_MS and COMBINE_WINDOW_MS.
 */

#ifndef MAIN_B394_DIGITALINPUT_H_
#define MAIN_B394_DIGITALINPUT_H_

void B394_DigitalInput_Init(void);

#if defined(B394)
void B394_DI_ConsoleWriteToActor_xface(void *msg);
#endif

#endif /* MAIN_B394_DIGITALINPUT_H_ */
