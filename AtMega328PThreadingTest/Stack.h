#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t last;
    uint8_t top;
} stack_header_t;

void *stack_create(uint8_t item_count, uint8_t data_size);
void stack_initialize(stack_header_t *header, uint8_t item_count);
void stack_destroy(void *stack_data);
bool stack_push(void *stack_data, void *data, uint8_t data_size);
bool stack_pop(void *stack_data, void *data, uint8_t data_size);
