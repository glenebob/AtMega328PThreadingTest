#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t last;
    uint8_t next_in;
    uint8_t next_out;
} ring_buffer_header_t;

void ring_buffer_initialize(ring_buffer_header_t *header, uint8_t item_count);
bool ring_buffer_push(void *buffer_data, void *data, uint8_t data_size);
bool ring_buffer_pop(void *buffer_data, void *data, uint8_t data_size);
bool ring_buffer_peek(void *buffer_data, void *data, uint8_t data_size);
bool ring_buffer_is_empty(void *buffer_data);
void ring_buffer_rotate(void *buffer_data, uint8_t data_size);
