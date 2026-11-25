#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"
#include "Context.h"
#include "Io.h"

#define EEPROM_LENGTH E2END + 1

typedef struct
{
    uint8_t reference_count;
    io_task_t io_task;
} eeprom_device_instance_t;

typedef struct
{
    const __flash device_function_table_t *function_table;
    eeprom_device_instance_t *instance;
} eeprom_device_t;

static device_t *eeprom_device_open(device_t *device);
static void eeprom_device_close(device_t *device);
static size_t eeprom_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t eeprom_device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static bool eeprom_device_info(device_t *device, uint8_t *flags, uint16_t *size);

static const __flash device_function_table_t eeprom_device_function_table =
{
    .open = eeprom_device_open,
    .close = eeprom_device_close,
    .read = eeprom_device_read,
    .write = eeprom_device_write,
    .info = eeprom_device_info,
};

eeprom_device_t eeprom_device_struct =
{
    .function_table = &eeprom_device_function_table,
    .instance = NULL
};

const device_t *eeprom_device = (device_t*) &eeprom_device_struct;

static device_t *eeprom_device_open(device_t *device_base)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    thread_lock();

    if (!device->instance)
    {
        device->instance = malloc(sizeof(eeprom_device_instance_t));

        if (!device->instance)
        {
            thread_unlock();

            return NULL;
        }

        device->instance->reference_count = 1;
        device->instance->io_task.status = IO_INACTIVE;
    }
    else
    {
        ++device->instance->reference_count;
    }

    thread_unlock();

    return device_base;
}

static void eeprom_device_close(device_t *device_base)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    thread_lock();

    if (!device->instance)
    {
        thread_unlock();

        return;
    }

    if (!--device->instance->reference_count)
    {
        free(device->instance);
        device->instance = NULL;
    }

    thread_unlock();
}

static bool eeprom_device_info(device_t *device, uint8_t *flags, uint16_t *size)
{
    if (!device)
    {
        return false;
    }

    *flags = DEVICE_READ | DEVICE_WRITE | DEVICE_SEEK;
    *size = EEPROM_LENGTH;

    return true;
}

static size_t eeprom_device_read(device_t *device_base, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    if (!device)
    {
        return -1;
    }

    if (offset > E2END)
    {
        return 0;
    }

    if (offset + count > EEPROM_LENGTH)
    {
        count = EEPROM_LENGTH - offset;
    }

    if (!count)
    {
        return 0;
    }

    cli();

    if (!device->instance)
    {
        sei();

        return -1;
    }

    if (device->instance->io_task.status)
    {
        exit(ERR_IO_READ_DUP);
    }

    device->instance->io_task.status = IO_READ;

    sei();

    uint8_t remaining = count;

    while (remaining--)
    {
        *(buffer++) = eeprom_read_byte((uint8_t*) offset++);
    }

    device->instance->io_task.status = IO_INACTIVE;

    return count;
}

static size_t eeprom_device_write(device_t *device_base, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    if (!device)
    {
        return -1;
    }

    if (offset > E2END)
    {
        return 0;
    }

    if (offset + count > EEPROM_LENGTH)
    {
        count = EEPROM_LENGTH - offset;
    }

    if (!count)
    {
        return 0;
    }

    cli();

    if (!device->instance)
    {
        sei();

        return -1;
    }

    if (device->instance->io_task.status)
    {
        exit(ERR_IO_WRITE_DUP);
    }

    device->instance->io_task.device_offset = offset;
    device->instance->io_task.buffer_offset = 0;

    if (!(EECR & (1 << EEPE)))
    {
        EEAR = (uint8_t) offset;
        EECR |= (1 << EERE);

        if (EEDR != buffer[device->instance->io_task.buffer_offset])
        {
            EEDR = buffer[device->instance->io_task.buffer_offset];

            EECR |= (1 << EEMPE);
            EECR |= (1 << EEPE);
        }

        ++device->instance->io_task.device_offset;
        ++device->instance->io_task.buffer_offset;

        if (device->instance->io_task.buffer_offset == count)
        {
            sei();

            return count;
        }
    }

    device->instance->io_task.status = IO_WRITE;
    device->instance->io_task.buffer = (uint8_t*) buffer;
    device->instance->io_task.buffer_length = count;

    EECR |= (1 << EERIE);

    int bytes_transferred = io_dispatch(&device->instance->io_task, timeout);

    EECR &= ~(1 << EERIE);

    sei();

    return bytes_transferred;
}

__attribute__((naked))
ISR(EE_READY_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    EEAR = (uint8_t) eeprom_device_struct.instance->io_task.device_offset;
    EECR |= (1 << EERE);

    if (EEDR != eeprom_device_struct.instance->io_task.buffer[eeprom_device_struct.instance->io_task.buffer_offset])
    {
        EEDR = eeprom_device_struct.instance->io_task.buffer[eeprom_device_struct.instance->io_task.buffer_offset];

        EECR |= (1 << EEMPE);
        EECR |= (1 << EEPE);
    }

    ++eeprom_device_struct.instance->io_task.device_offset;
    ++eeprom_device_struct.instance->io_task.buffer_offset;

    if (eeprom_device_struct.instance->io_task.buffer_offset != eeprom_device_struct.instance->io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    EECR &= ~(1 << EERIE);

    eeprom_device_struct.instance->io_task.status = IO_INACTIVE;

    if (eeprom_device_struct.instance->io_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(eeprom_device_struct.instance->io_task.timer_id);
    }

    thread_queue(eeprom_device_struct.instance->io_task.thread_id);

    if (!thread_select_if())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}
