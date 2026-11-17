#pragma once

#include <stdio.h>

#include "Io.h"

extern const device_t *uart;

void uart_initialize(uint32_t baud);
void uart_crash_init(uint32_t baud);
