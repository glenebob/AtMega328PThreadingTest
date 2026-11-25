#pragma once

#include <stdint.h>
#include <stdio.h>

#include "Types.h"

#define DEVICE_READ 1
#define DEVICE_WRITE 2
#define DEVICE_SEEK 4

typedef struct device_s device_t;

typedef device_t *(*device_open_f)(device_t *device);
typedef void (*device_close_f)(device_t *device);
typedef size_t (*device_read_f)(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
typedef size_t (*device_write_f)(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
typedef bool (*device_info_f)(device_t *device, uint8_t *flags, uint16_t *size);

typedef struct
{
    device_open_f open;
    device_close_f close;
    device_read_f read;
    device_write_f write;
    device_info_f info;
} device_function_table_t;

struct device_s
{
    const __flash device_function_table_t *function_table;
};

#define IO_INACTIVE 0
#define IO_READ 1
#define IO_WRITE 2

typedef struct
{
    uint8_t status;
    uint8_t *buffer;
    uint8_t buffer_length;
    uint8_t buffer_offset;
    uint16_t device_offset;
    uint8_t timer_id;
    uint8_t thread_id;
} io_task_t;

device_t *device_open(device_t *device);
void device_close(device_t *device);
size_t device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
size_t device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
int io_dispatch(io_task_t *io_task, uint16_t timeout);
