#pragma once

#include "Task.h"

void io_task_init();
int io_read(uint8_t *buffer, uint8_t buffer_length, uint16_t timeout);
int io_write(uint8_t *buffer, uint8_t buffer_length, uint16_t timeout);
void set_getchar_timeout(uint16_t timeout);
