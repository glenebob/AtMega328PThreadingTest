#pragma once

#include <stdio.h>

#include "Task.h"

void io_task_initialize(void);
void io_crash_init(int(*put)(char, struct __file *));
int io_read(uint8_t *buffer, uint8_t buffer_length, uint16_t timeout);
int io_write(uint8_t *buffer, uint8_t buffer_length, uint16_t timeout);
void set_getchar_timeout(uint16_t timeout);
