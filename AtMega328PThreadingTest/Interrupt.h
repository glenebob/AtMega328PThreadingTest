#pragma once

#include <stdlib.h>

#include <avr/interrupt.h>

#include "Error.h"

extern uint8_t interrupt_level;

#define INTERRUPT_INITIALIZE() interrupt_level = 1
#define INTERRUPT_RAISE_LEVEL() cli(); if (interrupt_level++ == 0xFF) exit(ERR_INT_RAISE_BAD_LEVEL)
#define INTERRUPT_LOWER_LEVEL() if (interrupt_level == 0) exit(ERR_INT_LOWER_BAD_LEVEL); if (--interrupt_level == 0) sei()
#define INTERRUPT_LOWER_LEVEL_NO_ENABLE() if (interrupt_level == 0) exit(ERR_INT_LOWER_BAD_LEVEL); --interrupt_level
