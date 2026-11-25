#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avr/pgmspace.h>

#include "Types.h"
#include "Io.h"

static size_t flash_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static bool flash_device_info(device_t *device, uint8_t *flags, uint16_t *size);

static const __flash device_function_table_t flash_device_function_table =
{
    .open = NULL,
    .close = NULL,
    .read = flash_device_read,
    .write = NULL,
    .info = flash_device_info
};

static const device_t flash_device_struct =
{
    .function_table = &flash_device_function_table
};

const device_t *flash_device = &flash_device_struct;

static bool flash_device_info(device_t *device, uint8_t *flags, uint16_t *size)
{
    if (!device)
    {
        return false;
    }

    *flags = DEVICE_READ | DEVICE_SEEK;
    *size = (uint16_t) FLASHEND + 1;
    
    return true;
}

static size_t flash_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    if (!device)
    {
        return -1;
    }

    if (offset > FLASHEND)
    {
        return 0;
    }

    if (offset + count > (uint16_t) FLASHEND + 1)
    {
        count = (uint16_t) FLASHEND + 1 - offset;
    }

    memcpy_P(buffer, (void*) offset, count);
    
    return count;
}
