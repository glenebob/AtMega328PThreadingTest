#pragma once

#include <stdio.h>

#include "Io.h"

extern device_t *uart;

void uart_initialize(void);
void io_crash_init(void);
void set_getchar_timeout(uint16_t timeout);
