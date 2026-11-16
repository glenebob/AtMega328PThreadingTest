#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <avr/io.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"

#include "Io.h"

#define FLAGS_READ 0b00000001
#define FLAGS_WRITE 0b00000010
#define FLAGS_SEEK 0b00000100

typedef struct
{
    uint8_t flags;
} file_user_data_t;

extern size_t __real_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t __real_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

uint8_t get_function_offset(uint8_t flags, uint8_t mask, uint8_t position)
{
    return ((flags & mask) >> position) * sizeof(uintptr_t) - 1;
}

uintptr_t get_function_address_from_device(const device_t *device, uint8_t mask, uint8_t position)
{
    uint8_t offset = get_function_offset(device->flags, mask, position);

    if (offset != 0xFF)
    {
        return *((uintptr_t*) ((uint8_t*) device + offset));
    }
    else
    {
        return 0;
    }
}

size_t device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (device == NULL)
    {
        return -1;
    }

    device_read_f device_read = (device_read_f) get_function_address_from_device(device, 0b00000011, 0);

    if (!device_read)
    {
        return -1;
    }

    return device_read(device, buffer, count, timeout);
}

size_t device_write(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (device == NULL)
    {
        return -1;
    }

    device_write_f device_write = (device_write_f) get_function_address_from_device(device, 0b00001100, 2);

    if (!device_write)
    {
        return -1;
    }

    return device_write(device, buffer, count, timeout);
}

bool device_seek(device_t *device, size_t offset, int8_t whence)
{
    if (device == NULL)
    {
        return false;
    }

    device_seek_f device_seek = (device_seek_f) get_function_address_from_device(device, 0b00110000, 4);

    if (!device_seek)
    {
        return false;
    }

    return device_seek(device, offset, whence);
}

int file_getchar(FILE *stream)
{
    device_t *provider = (device_t*) stream->udata;
    char c;
    size_t result = device_read(provider, (uint8_t*) &c, 1, 0);
    
    if (result == -1)
    {
        return _FDEV_ERR;
    }
    else if (result == 0)
    {
        return _FDEV_EOF;
    }
    else
    {
        return (uint8_t) c;
    }
}

int file_putchar(char c, FILE *stream)
{
    device_t *provider = (device_t*) stream->udata;
    size_t result = device_write(provider, (uint8_t*) &c, sizeof(uint8_t), 0);
    
    if (result == -1)
    {
        return _FDEV_ERR;
    }
    else if (result == 0)
    {
        return _FDEV_EOF;
    }                
    else
    {
        return 0;
    }        
}

size_t file_read(void *buffer, size_t size, size_t nmemb, FILE *stream, uint16_t timeout)
{
    device_t *provider = (device_t*) stream->udata;
    size_t count_in = size * nmemb;
    
    if (count_in > 0xFF)
    {
        count_in = 0xFF;
    }

    return device_read(provider, buffer, (uint8_t) count_in, timeout);
}

size_t file_write(const void *buffer, size_t size, size_t nmemb, FILE *stream, uint16_t timeout)
{
    device_t *provider = (device_t*) stream->udata;
    size_t count_in = size * nmemb;
    
    if (count_in > 0xFF)
    {
        count_in = 0xFF;
    }

    return device_write(provider, buffer, (uint8_t) count_in, timeout);
}

int file_seek(FILE *stream, long offset, int whence)
{
    device_t *provider = (device_t*) stream->udata;

    return device_seek(provider, offset, whence) ? 0 : -1;
}

size_t freadt(void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout)
{
    if (stream == NULL)
    {
        return -1;
    }

    if (!(stream->flags & __SRD))
    {
        return -1;
    }

    return file_read(ptr, size, nmemb, stream, timeout);
}

size_t fwritet(const void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout)
{
    if (stream == NULL)
    {
        return -1;
    }

    if (!(stream->flags & __SWR))
    {
        return -1;
    }

    return file_write(ptr, size, nmemb, stream, timeout);
}

size_t __wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return freadt(ptr, size, nmemb, stream, 0);
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwritet(ptr, size, nmemb, stream, 0);
}

int fseek(FILE *stream, long offset, int whence)
{
    if (stream == NULL)
    {
        return -1;
    }

    return file_seek(stream, offset, whence);
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

    int return_value = io_task->offset < io_task->buffer_length ? -1 : io_task->buffer_length;

    io_task->status = IO_INACTIVE;

    return return_value;
}
