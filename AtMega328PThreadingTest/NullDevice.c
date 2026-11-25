#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avr/pgmspace.h>

#include "Types.h"
#include "Io.h"

static size_t null_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t null_zero_device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t zero_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static bool null_zero_device_info(device_t *device, uint8_t *flags, uint16_t *size);

static const __flash device_function_table_t null_device_function_table =
{
    .open = NULL,
    .close = NULL,
    .read = null_device_read,
    .write = null_zero_device_write,
    .info = null_zero_device_info
};

static const __flash device_function_table_t zero_device_function_table =
{
    .open = NULL,
    .close = NULL,
    .read = zero_device_read,
    .write = null_zero_device_write,
    .info = null_zero_device_info
};

static const device_t null_device_struct =
{
    .function_table = &null_device_function_table
};

static const device_t zero_device_struct =
{
    .function_table = &zero_device_function_table
};

const device_t *null_device = &null_device_struct;
const device_t *zero_device = &zero_device_struct;

static bool null_zero_device_info(device_t *device, uint8_t *flags, uint16_t *size)
{
    if (!device)
    {
        return false;
    }

    *flags = DEVICE_READ | DEVICE_WRITE;
    *size = 0;

    return true;
}

static size_t null_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (!device)
    {
        return -1;
    }

    return 0;
}

static size_t null_zero_device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (!device)
    {
        return -1;
    }

    return count;
}

static size_t zero_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (!device)
    {
        return -1;
    }

    memset(buffer, 0, count);
        
    return count;
}
