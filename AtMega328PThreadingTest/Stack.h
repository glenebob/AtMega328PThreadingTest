#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t last;
    uint8_t top;
} stack_header_t;

void stack_initialize(stack_header_t *header, uint8_t item_count);
bool stack_push(void *stack_data, void *data, uint8_t data_size);
bool stack_pop(void *stack_data, void *data, uint8_t data_size);
