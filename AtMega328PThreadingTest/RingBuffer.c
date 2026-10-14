#include <stdint.h>
#include <string.h>

#include "Types.h"

#include "RingBuffer.h"

typedef struct
{
    ring_buffer_header_t header;
    uint8_t data[1];
} ring_buffer_t;

void ring_buffer_initialize(ring_buffer_header_t *header, uint8_t item_count)
{
    header->last = item_count - 1;
    header->next_in = 0;
    header->next_out = 0xFF;
}

bool ring_buffer_push(void *buffer_data, void *data, uint8_t data_size)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    if (buffer->header.next_in == buffer->header.next_out)
    {
        return false;
    }

    memcpy(&buffer->data[buffer->header.next_in * data_size], data, data_size);

    if (buffer->header.next_out == 0xFF)
    {
        buffer->header.next_out = buffer->header.next_in;
    }

    if (buffer->header.next_in == buffer->header.last)
    {
        buffer->header.next_in = 0;
    }
    else
    {
        ++buffer->header.next_in;
    }

    return true;
}

bool ring_buffer_pop(void *buffer_data, void *data, uint8_t data_size)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    if (buffer->header.next_out == 0xFF)
    {
        return false;
    }

    memcpy(data, &buffer->data[buffer->header.next_out * data_size], data_size);

    ++buffer->header.next_out;

    if (buffer->header.next_out > buffer->header.last)
    {
        buffer->header.next_out = 0;
    }

    if (buffer->header.next_out == buffer->header.next_in)
    {
        buffer->header.next_out = 0xFF;
    }

    return true;
}

bool ring_buffer_peek(void *buffer_data, void *data, uint8_t data_size)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    if (buffer->header.next_out == 0xFF)
    {
        return false;
    }

    memcpy(data, &buffer->data[buffer->header.next_out * data_size], data_size);

    return true;
}

bool ring_buffer_is_empty(void *buffer_data)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    return buffer->header.next_out == 0xFF;
}

void ring_buffer_rotate(void *buffer_data, uint8_t data_size)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    if (buffer->header.next_out == 0xFF)
    {
        return;
    }
    else if (buffer->header.next_in == 0 && buffer->header.next_out == buffer->header.last)
    {
        return;
    }
    else if (buffer->header.next_in == buffer->header.next_out + 1)
    {
        return;
    }

    memcpy(&buffer->data[buffer->header.next_in * data_size], &buffer->data[buffer->header.next_out * data_size], data_size);

    if (buffer->header.next_in == buffer->header.last)
    {
        buffer->header.next_in = 0;
    }
    else
    {
        ++buffer->header.next_in;
    }

    if (buffer->header.next_out == buffer->header.last)
    {
        buffer->header.next_out = 0;
    }
    else
    {
        ++buffer->header.next_out;
    }
}
