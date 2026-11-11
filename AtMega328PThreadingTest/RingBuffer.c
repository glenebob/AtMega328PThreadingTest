#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Types.h"
#include "Thread.h"

#include "RingBuffer.h"

typedef struct
{
    ring_buffer_header_t header;
    uint8_t data[];
} ring_buffer_t;

void *ring_buffer_create(uint8_t item_count, uint8_t data_size)
{
    thread_lock();

    void *ring_buffer_data = malloc(sizeof(ring_buffer_t) + item_count * data_size);

    thread_unlock();

    if (!ring_buffer_data)
    {
        return ring_buffer_data;
    }

    ring_buffer_initialize(ring_buffer_data, item_count);

    return ring_buffer_data;
}

void ring_buffer_initialize(ring_buffer_header_t *header, uint8_t item_count)
{
    header->last = item_count - 1;
    header->next_in = 0;
    header->next_out = 0xFF;
}

void ring_buffer_destroy(void *ring_buffer_data)
{
    thread_lock();

    free(ring_buffer_data);

    thread_unlock();
}

bool ring_buffer_push(void *buffer_data, void *data, uint8_t data_size, bool overwrite_if_full)
{
    ring_buffer_t *buffer = (ring_buffer_t*) buffer_data;

    if (buffer->header.next_in == buffer->header.next_out)
    {
        if (overwrite_if_full)
        {
            if (buffer->header.next_out == buffer->header.last)
            {
                buffer->header.next_out = 0;
            }
            else
            {
                ++buffer->header.next_out;
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (buffer->header.next_out == 0xFF)
        {
            buffer->header.next_out = buffer->header.next_in;
        }
    }

    if (data_size == 1)
    {
        memcpy(&buffer->data[buffer->header.next_in], data, 1);
    }
    else
    {
        memcpy(&buffer->data[buffer->header.next_in * data_size], data, data_size);
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

    if (data_size == 1)
    {
        memcpy(data, &buffer->data[buffer->header.next_out], 1);
    }
    else
    {
        memcpy(data, &buffer->data[buffer->header.next_out * data_size], data_size);
    }

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

    if (data_size == 1)
    {
        memcpy(data, &buffer->data[buffer->header.next_out], 1);
    }
    else
    {
        memcpy(data, &buffer->data[buffer->header.next_out * data_size], data_size);
    }

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

    if (data_size == 1)
    {
        memcpy(&buffer->data[buffer->header.next_in], &buffer->data[buffer->header.next_out], 1);
    }
    else
    {
        memcpy(&buffer->data[buffer->header.next_in * data_size], &buffer->data[buffer->header.next_out * data_size], data_size);
    }

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
