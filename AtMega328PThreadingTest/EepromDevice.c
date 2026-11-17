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
    const __flash device_function_table_t *function_table;
    uint8_t reference_count;
    io_task_t io_task;
} eeprom_device_t;

static size_t eeprom_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t eeprom_device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static void eeprom_device_info(device_t *device, uint8_t *flags, uint16_t *size);

static const __flash device_function_table_t eeprom_device_function_table =
{
    .read = eeprom_device_read,
    .write = eeprom_device_write,
    .info = eeprom_device_info
};

device_t *eeprom_device;

void eeprom_initialize(void)
{
    thread_lock();

    if (!eeprom_device)
    {
        eeprom_device_t *new_eeprom_device = malloc(sizeof(eeprom_device_t));

        if (!new_eeprom_device)
        {
            thread_unlock();

            return;
        }

        new_eeprom_device->function_table = &eeprom_device_function_table;
        new_eeprom_device->reference_count = 1;
        new_eeprom_device->io_task.status = IO_INACTIVE;

        eeprom_device = (device_t*) new_eeprom_device;
    }
    else
    {
        ++eeprom_device->reference_count;
    }

    thread_unlock();
}

static void eeprom_device_info(device_t *device, uint8_t *flags, uint16_t *size)
{
    *flags = DEVICE_READ | DEVICE_WRITE | DEVICE_SEEK;
    *size = EEPROM_LENGTH;
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

    if (device->io_task.status)
    {
        exit(ERR_IO_READ_DUP);
    }

    device->io_task.status = IO_READ;

    sei();

    uint8_t remaining = count;

    while (remaining--)
    {
        *(buffer++) = eeprom_read_byte((uint8_t*) offset++);
    }

    cli();

    device->io_task.status = IO_INACTIVE;

    sei();

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

    if (device->io_task.status)
    {
        exit(ERR_IO_WRITE_DUP);
    }

    device->io_task.device_offset = offset;
    device->io_task.buffer_offset = 0;

    if (!(EECR & (1 << EEPE)))
    {
        EEAR = (uint8_t) offset;
        EECR |= (1 << EERE);

        if (EEDR != buffer[device->io_task.buffer_offset])
        {
            EEDR = buffer[device->io_task.buffer_offset];

            EECR |= (1 << EEMPE);
            EECR |= (1 << EEPE);
        }

        ++device->io_task.device_offset;
        ++device->io_task.buffer_offset;

        if (device->io_task.buffer_offset == count)
        {
            sei();

            return count;
        }
    }

    device->io_task.status = IO_WRITE;
    device->io_task.buffer = (uint8_t*) buffer;
    device->io_task.buffer_length = count;

    EECR |= (1 << EERIE);

    int bytes_transferred = io_dispatch(&device->io_task, timeout);

    EECR &= ~(1 << EERIE);

    sei();

    return bytes_transferred;
}

__attribute__((naked))
ISR(EE_READY_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    eeprom_device_t *device = (eeprom_device_t*) eeprom_device;

    EEAR = (uint8_t) device->io_task.device_offset;
    EECR |= (1 << EERE);

    if (EEDR != device->io_task.buffer[device->io_task.buffer_offset])
    {
        EEDR = device->io_task.buffer[device->io_task.buffer_offset];

        EECR |= (1 << EEMPE);
        EECR |= (1 << EEPE);
    }

    ++device->io_task.device_offset;
    ++device->io_task.buffer_offset;

    if (device->io_task.buffer_offset != device->io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    EECR &= ~(1 << EERIE);

    device->io_task.status = IO_INACTIVE;

    if (device->io_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(device->io_task.timer_id);
    }

    thread_queue(device->io_task.thread_id);

    if (!thread_select_if())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}
