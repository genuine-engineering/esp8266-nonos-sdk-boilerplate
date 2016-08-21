#ifndef _LED_H_
#define _LED_H_
#include "os_type.h"

void led_init();
void led_write(uint8_t value);
void led_blink(uint32_t on_time, uint32_t off_time);
#endif
