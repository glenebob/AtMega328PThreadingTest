#include <stdio.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"
#include "Context.h"
#include "Io.h"

#define EEPROM_LENGTH E2END + 1

typedef struct
{
    uint8_t flags;
    device_read_f read;
    device_write_f write;
    device_seek_f seek;
    uint16_t position;
    io_task_t io_task;
} eeprom_device_t;

static size_t eeprom_device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t eeprom_device_write(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static bool eeprom_device_seek(device_t *device, int16_t offset, int8_t whence);

static eeprom_device_t *eeprom_device;

device_t *open_eeprom_device(void)
{
    cli();

    if (eeprom_device)
    {
        sei();

        return NULL;
    }

    thread_lock();

    eeprom_device_t *device = malloc(sizeof(eeprom_device_t));

    thread_unlock();

    device->flags = 0b00111001;
    device->read = eeprom_device_read;
    device->write = eeprom_device_write;
    device->seek = eeprom_device_seek;
    device->position = 0;
    device->io_task.status = IO_INACTIVE;

    eeprom_device = device;

    return (device_t*) device;
}

FILE *open_eeprom_file(device_t *device)
{
    if (!device)
    {
        return NULL;
    }

    thread_lock();

    FILE *file = malloc(sizeof(FILE));

    thread_unlock();

    fdev_setup_stream(file, file_putchar, file_getchar, _FDEV_SETUP_RW | __SMALLOC | __SRD | __SWR);

    file->udata = device;

    return file;
}

static size_t eeprom_device_read(device_t *device_base, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    if (!device)
    {
        return -1;
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

    if (device->position + count > EEPROM_LENGTH)
    {
        return count = EEPROM_LENGTH - device->position;
    }

    uint8_t remaining = count;

    while (remaining--)
    {
        *(buffer++) = eeprom_read_byte((uint8_t*) device->position++);
    }

    cli();

    device->io_task.status = IO_INACTIVE;

    sei();

    return count;
}

static size_t eeprom_device_write(device_t *device_base, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    if (!device)
    {
        return -1;
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

    device->io_task.offset = 0;

    if (!(EECR & (1 << EEPE)))
    {
        EEAR = (uint8_t) device->position;
        EECR |= (1 << EERE);

        if (EEDR != buffer[device->io_task.offset])
        {
            EEDR = buffer[device->io_task.offset];

            EECR |= (1 << EEMPE);
            EECR |= (1 << EEPE);
        }

        ++device->position;
        ++device->io_task.offset;

        if (device->io_task.offset == count)
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

    EEAR = (uint8_t) eeprom_device->position;
    EECR |= (1 << EERE);

    if (EEDR != eeprom_device->io_task.buffer[eeprom_device->io_task.offset])
    {
        EEDR = eeprom_device->io_task.buffer[eeprom_device->io_task.offset];

        EECR |= (1 << EEMPE);
        EECR |= (1 << EEPE);
    }

    ++eeprom_device->position;
    ++eeprom_device->io_task.offset;

    if (eeprom_device->io_task.offset != eeprom_device->io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    EECR &= ~(1 << EERIE);

    eeprom_device->io_task.status = IO_INACTIVE;

    if (eeprom_device->io_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(eeprom_device->io_task.timer_id);
    }

    thread_queue(eeprom_device->io_task.thread_id);

    if (!thread_select_if())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}

static bool eeprom_device_seek(device_t *device_base, int16_t offset, int8_t whence)
{
    eeprom_device_t *device = (eeprom_device_t*) device_base;

    switch (whence)
    {
        case SEEK_SET:
            if (offset < 0 || offset >= EEPROM_LENGTH)
            {
                return false;
            }

            break;

        case SEEK_CUR:
        {
            offset = device->position + offset;

            if (offset < 0)
            {
                return false;
            }
            else
            {
                if (offset >= EEPROM_LENGTH)
                {
                    return false;
                }
            }

            break;
        }

        case SEEK_END:
            if (offset > 0)
            {
                return false;
            }

            offset = EEPROM_LENGTH - offset - 1;

            if (offset < 0)
            {
                return false;
            }

            break;
    }

    cli();

    device->position = (uint16_t) offset;

    sei();

    return true;
}
