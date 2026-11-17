#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"

#include "Io.h"

size_t device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (device == NULL)
    {
        return -1;
    }

    return device->function_table->read(device, offset, buffer, count, timeout);
}

size_t device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (device == NULL)
    {
        return -1;
    }

    return device->function_table->write(device, offset, buffer, count, timeout);
}


int io_dispatch(io_task_t *io_task, uint16_t timeout)
{
    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    io_task->thread_id = current_thread_id;

    if (timeout > 0)
    {
        io_task->timer_id = timer_add(current_thread_id, timeout);
    }
    else
    {
        io_task->timer_id = OBJECT_ID_INVALID;
    }

    thread_block_and_select();
    thread_yield_dispatch();

    int return_value = io_task->buffer_offset < io_task->buffer_length ? -1 : io_task->buffer_length;

    io_task->status = IO_INACTIVE;

    return return_value;
}
