#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "F_CPU.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"
#include "Context.h"
#include "Io.h"

#include "UartDevice.h"

typedef struct
{
    uint8_t reference_count;
    io_task_t io_task;
} uart_device_instance_t;

typedef struct
{
    const __flash device_function_table_t *function_table;
    uint8_t flags;
    uart_device_instance_t *instance;
} uart_device_t;

static device_t *uart_device_open(device_t *device);
static void uart_device_close(device_t *device);
static size_t uart_device_read(device_t *device, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t uart_device_write(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t uart_device_write_spin(device_t *device_base, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static bool uart_device_info(device_t *device, uint8_t *flags, uint16_t *size);
static bool uart_device_info_spin(device_t *device, uint8_t *flags, uint16_t *size);

static const __flash device_function_table_t uart_device_function_table =
{
    .open = uart_device_open,
    .close = uart_device_close,
    .read = uart_device_read,
    .write = uart_device_write,
    .info = uart_device_info
};

static const __flash device_function_table_t uart_device_function_table_spin =
{
    .open = uart_device_open,
    .close = NULL,
    .read = NULL,
    .write = uart_device_write_spin,
    .info = uart_device_info_spin
};

static uart_device_t uart_device_struct =
{
    .function_table = NULL,
    .instance = NULL
};

const device_t *uart_device = (device_t*) &uart_device_struct;

static uint16_t calculate_baud_rate(uint32_t baud)
{
    return (uint16_t) ((F_CPU) / (baud * 16UL) -1);
}

void uart_initialize(uint32_t baud, uint8_t flags)
{
    uint16_t baud_rate = calculate_baud_rate(baud);

    // Set baud rate
    UBRR0H = (uint8_t) (baud_rate >> 8);
    UBRR0L = (uint8_t) baud_rate;
    // Set frame format: 8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    // Enable the receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    uart_device_struct.function_table = &uart_device_function_table;
    uart_device_struct.flags = flags;
};

void uart_crash_initialize(uint32_t baud, uint8_t flags)
{
    uint16_t baud_rate = calculate_baud_rate(baud);

    // Set baud rate
    UBRR0H = (uint8_t) (baud_rate >> 8);
    UBRR0L = (uint8_t) baud_rate;
    // Set frame format: 8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    // Enable the transmitter only
    UCSR0B = (1 << TXEN0);

    uart_device_struct.function_table = &uart_device_function_table_spin;
    uart_device_struct.flags = flags;

    if (uart_device_struct.instance)
    {
        uart_device_struct.instance->io_task.status = IO_INACTIVE;
    }
}

device_t *uart_device_open(device_t *device_base)
{
    uart_device_t *device = (uart_device_t*) device_base;

    thread_lock();

    if (!device->instance)
    {
        device->instance = malloc(sizeof(uart_device_instance_t));

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

static void uart_device_close(device_t *device_base)
{
    uart_device_t *device = (uart_device_t*) device_base;

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

bool uart_device_info(device_t *device, uint8_t *flags, uint16_t *size)
{
    if (!device)
    {
        return false;
    }

    *flags = DEVICE_READ | DEVICE_WRITE;
    *size = 0;

    return true;
}

bool uart_device_info_spin(device_t *device, uint8_t *flags, uint16_t *size)
{
    if (!device)
    {
        return false;
    }

    *flags = DEVICE_WRITE;
    *size = 0;

    return true;
}

static size_t uart_device_read(device_t *device_base, uint16_t offset, uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    uart_device_t *device = (uart_device_t*) device_base;

    if (!device)
    {
        return 0xFF;
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

    device->instance->io_task.buffer_offset = 0;

    if (UCSR0A & (1 << RXC0))
    {
        uint8_t b = UDR0;

        if (b == 0x0D)
        {
            if (!(device->flags & UART_RAW))
            {
                b = 0x0A;
            }
        }

        buffer[device->instance->io_task.buffer_offset++] = b;

        if (device->instance->io_task.buffer_offset == count)
        {
            sei();

            return count;
        }
    }

    device->instance->io_task.status = IO_READ;
    device->instance->io_task.buffer = buffer;
    device->instance->io_task.buffer_length = count;

    UCSR0B |= (1 << RXCIE0);

    int bytes_transferred = io_dispatch(&device->instance->io_task, timeout);

    UCSR0B &= ~(1 << RXCIE0);

    sei();

    return bytes_transferred;
}

static size_t uart_device_write(device_t *device_base, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    uart_device_t *device = (uart_device_t*) device_base;
    
    if (!device)
    {
        return 0xFF;
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

    device->instance->io_task.buffer_offset = 0;

    if (UCSR0A & (1 << UDRE0))
    {
        UDR0 = buffer[device->instance->io_task.buffer_offset++];
        
        if (device->instance->io_task.buffer_offset == count)
        {
            sei();

            return count;
        }
    }

    device->instance->io_task.status = IO_WRITE;
    device->instance->io_task.buffer = (uint8_t*) buffer;
    device->instance->io_task.buffer_length = count;

    UCSR0B |= (1 << UDRIE0);

    int bytes_transferred = io_dispatch(&device->instance->io_task, timeout);

    UCSR0B &= ~(1 << UDRIE0);

    sei();

    return bytes_transferred;
}

static void uart_device_write_byte_spin(uint8_t byte)
{
    while (!(UCSR0A & (1 << UDRE0)));

    UDR0 = byte;
}

static void uart_device_write_byte_with_behavior_spin(uint8_t byte)
{
    if (byte == 0x0A)
    {
        uart_device_write_byte_spin(0x0D);
    }

    uart_device_write_byte_spin(byte);
 }

static size_t uart_device_write_spin(device_t *device, uint16_t offset, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    uint8_t remaining  = count;
    
    while (remaining--)
    {
        uart_device_write_byte_with_behavior_spin(*(buffer++));
    }

    return count;
}

__attribute__((naked))
static void io_interrupt_common(void)
{
    uart_device_struct.instance->io_task.status = IO_INACTIVE;

    if (uart_device_struct.instance->io_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(uart_device_struct.instance->io_task.timer_id);
    }

    thread_queue(uart_device_struct.instance->io_task.thread_id);

    if (!thread_select_if())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}

__attribute__((naked))
ISR(USART_RX_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    uint8_t b = UDR0;

    if (b == 0x0D)
    {
        if (!(uart_device_struct.flags & UART_RAW))
        {
            b = 0x0A;
        }
    }

    uart_device_struct.instance->io_task.buffer[uart_device_struct.instance->io_task.buffer_offset++] = b;

    if (uart_device_struct.instance->io_task.buffer_offset != uart_device_struct.instance->io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    UCSR0B &= ~(1 << RXCIE0);

    JUMP(io_interrupt_common);
}

__attribute__((naked))
ISR(USART_UDRE_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    UDR0 = uart_device_struct.instance->io_task.buffer[uart_device_struct.instance->io_task.buffer_offset++];

    if (uart_device_struct.instance->io_task.buffer_offset != uart_device_struct.instance->io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    UCSR0B &= ~(1 << UDRIE0);

    JUMP(io_interrupt_common);
}
