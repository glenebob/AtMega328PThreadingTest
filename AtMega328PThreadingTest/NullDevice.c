#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Io.h"

typedef struct
{
    uint8_t flags;
    device_read_f read;
    device_write_f write;
} null_zero_device_t;

null_zero_device_t null_device_struct;
null_zero_device_t zero_device_struct;
FILE null_file_struct;
FILE zero_file_struct;

device_t *null_device;
device_t *zero_device;
FILE *null_file;
FILE *zero_file;

static size_t null_device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t null_zero_device_write(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t zero_device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);

void null_device_initialize(void)
{
    null_device_struct.flags = 0b00001001;
    null_device_struct.read = null_device_read;
    null_device_struct.write = null_zero_device_write;
    
    null_device = (device_t*) &null_device_struct;

    zero_device_struct.flags = 0b00001001;
    zero_device_struct.read = zero_device_read;
    zero_device_struct.write = null_zero_device_write;
    
    zero_device = (device_t*) &zero_device_struct;

    fdev_setup_stream(&null_file_struct, file_putchar, file_getchar, _FDEV_SETUP_RW | __SRD | __SWR);
    
    null_file_struct.udata = null_device;

    null_file = &null_file_struct;

    fdev_setup_stream(&zero_file_struct, file_putchar, file_getchar, _FDEV_SETUP_RW | __SRD | __SWR);
    
    zero_file_struct.udata = zero_device;
    
    zero_file = &zero_file_struct;
}

static size_t null_device_read(device_t *device_base, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    return 0;
}

static size_t null_zero_device_write(device_t *device_base, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    return count;
}

static size_t zero_device_read(device_t *device_base, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    memset(buffer, 0, count);
        
    return count;
}
