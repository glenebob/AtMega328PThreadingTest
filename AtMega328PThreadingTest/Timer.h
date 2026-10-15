#pragma once

#include <stdint.h>

void timers_initialize(void);
uint8_t timer_add(uint8_t thread_id, uint16_t ticks);
void timer_cancel(uint8_t timer_id);
void advance_active_timers(void);
