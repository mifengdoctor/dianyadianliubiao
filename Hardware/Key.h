#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

#define KEY_EVENT_K1    0x01U
#define KEY_EVENT_K2    0x02U
#define KEY_EVENT_K3    0x04U

void Key_Init(void);
void Key_UpdateISR(void);
uint8_t Key_GetEvents(void);

#endif
