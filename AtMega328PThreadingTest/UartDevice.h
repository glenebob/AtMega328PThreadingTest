#pragma once

#include <stdio.h>

#include "Io.h"

#define UART_RAW 1

extern const device_t *uart_device;

void uart_initialize(uint32_t baud, uint8_t flags);
void uart_crash_init(uint32_t baud, uint8_t flags);
