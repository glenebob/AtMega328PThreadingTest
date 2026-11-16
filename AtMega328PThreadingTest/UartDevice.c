#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "F_CPU.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

#include "Types.h"
#include "Error.h"
#include "Timer.h"
#include "Thread.h"
#include "Context.h"
#include "Io.h"

#include "UartDevice.h"

//#define BAUD 9600
//#define BAUD 62500
//#define BAUD 125000
#define BAUD 250000
#define BAUDRATE ((F_CPU) / (BAUD * 16UL) -1)

typedef struct
{
    uint8_t flags;
    device_read_f read;
    device_write_f write;
    io_task_t io_task;
} uart_device_t;

uart_device_t uart_device_struct;
device_t *uart_device;

FILE uart_file_read_struct;
FILE uart_file_write_struct;

static size_t uart_device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t uart_device_write(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static size_t uart_device_write_spin(device_t *device_base, const uint8_t *buffer, uint8_t count, uint16_t timeout);
static int stdio_getchar(FILE *stream);
static int stdio_putchar(char c, FILE *stream);

static uint16_t default_read_timeout;

void uart_initialize()
{
//    io_task.status = IO_INACTIVE;

    // Set baud rate
    UBRR0H = (uint8_t) (BAUDRATE >> 8);
    UBRR0L = (uint8_t) BAUDRATE;
    // Set frame format: 8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    // Enable the receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    uart_device_struct.flags = 0b00001001;
    uart_device_struct.read = uart_device_read;
    uart_device_struct.write = uart_device_write;
    uart_device_struct.io_task.status = IO_INACTIVE;

    uart_device = (device_t*) &uart_device_struct;

    fdev_setup_stream(&uart_file_read_struct, NULL, stdio_getchar, _FDEV_SETUP_READ | __SRD);

    uart_file_read_struct.udata = uart_device;

    fdev_setup_stream(&uart_file_write_struct, stdio_putchar, NULL, _FDEV_SETUP_WRITE | __SWR);

    uart_file_write_struct.udata = uart_device;
    
    stdin = &uart_file_read_struct;
    stdout = &uart_file_write_struct;
    stderr = &uart_file_write_struct;

    default_read_timeout = 0;
};

void io_crash_init(void)
{
    // Set baud rate
    UBRR0H = (uint8_t) (BAUDRATE >> 8);
    UBRR0L = (uint8_t) BAUDRATE;
    // Set frame format: 8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    // Enable the transmitter only
    UCSR0B = (1 << TXEN0);

    uart_device_struct.flags = 0b00001000;
    uart_device_struct.read = NULL;
    uart_device_struct.write = uart_device_write_spin;
    uart_device_struct.io_task.status = IO_INACTIVE;
}

static size_t uart_device_read(device_t *device_base, uint8_t *buffer, uint8_t count, uint16_t timeout)
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

    if (device->io_task.status)
    {
        exit(ERR_IO_READ_DUP);
    }

    device->io_task.offset = 0;

    if (UCSR0A & (1 << RXC0))
    {
        buffer[device->io_task.offset++] = UDR0;

        if (device->io_task.offset == count)
        {
            sei();

            return count;
        }
    }

    device->io_task.status = IO_READ;
    device->io_task.buffer = buffer;
    device->io_task.buffer_length = count;

    UCSR0B |= (1 << RXCIE0);

    int bytes_transferred = io_dispatch(&device->io_task, timeout);

    UCSR0B &= ~(1 << RXCIE0);

    sei();

    return bytes_transferred;
}

static size_t uart_device_write(device_t *device_base, const uint8_t *buffer, uint8_t count, uint16_t timeout)
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

    if (device->io_task.status)
    {
        exit(ERR_IO_WRITE_DUP);
    }

    device->io_task.offset = 0;

    if (UCSR0A & (1 << UDRE0))
    {
        UDR0 = buffer[device->io_task.offset++];
        
        if (device->io_task.offset == count)
        {
            sei();
            
            return count;
        }
    }

    device->io_task.status = IO_WRITE;
    device->io_task.buffer = (uint8_t*) buffer;
    device->io_task.buffer_length = count;

    UCSR0B |= (1 << UDRIE0);

    int bytes_transferred = io_dispatch(&device->io_task, timeout);

    UCSR0B &= ~(1 << UDRIE0);

    sei();

    return bytes_transferred;
}

static size_t uart_device_write_spin(device_t *device_base, const uint8_t *buffer, uint8_t count, uint16_t timeout)
{
    uint8_t remaining  = count;
    
    while (remaining--)
    {
        while (!(UCSR0A & (1 << UDRE0)));

        UDR0 = *(buffer++);
    }

    return count;
}

void set_getchar_timeout(uint16_t timeout)
{
    default_read_timeout = timeout;
}

__attribute__((naked))
static void io_interrupt_common(void)
{
    uart_device_struct.io_task.status = IO_INACTIVE;

    if (uart_device_struct.io_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(uart_device_struct.io_task.timer_id);
    }

    thread_queue(uart_device_struct.io_task.thread_id);

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

    uart_device_struct.io_task.buffer[uart_device_struct.io_task.offset++] = UDR0;

    if (uart_device_struct.io_task.offset != uart_device_struct.io_task.buffer_length)
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

    UDR0 = uart_device_struct.io_task.buffer[uart_device_struct.io_task.offset++];

    if (uart_device_struct.io_task.offset != uart_device_struct.io_task.buffer_length)
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    UCSR0B &= ~(1 << UDRIE0);

    JUMP(io_interrupt_common);
}

static int stdio_getchar(FILE *stream)
{
    device_t *provider = (device_t*) stream->udata;
    char c;
    size_t result = device_read(provider, (uint8_t*) &c, 1, default_read_timeout);
    
    if (result == SIZE_MAX)
    {
        return -1;
    }
    else
    {
        // PuTTY hack. It always sends \r for EOL. No apparent way to configure it.

        if (c == '\r')
        {
            c = '\n';
        }
        
        return c;
    }
}

static int stdio_putchar(char c, FILE *stream)
{
    device_t *provider = (device_t*) stream->udata;

    if (c == '\n')
    {
        char buffer[2];

        buffer[0] = '\r';
        buffer[1] = c;
        
        device_write(provider, (uint8_t*) buffer, sizeof(buffer), 0);
    }
    else
    {
        device_write(provider, (uint8_t*) &c, sizeof(uint8_t), 0);
    }

    return 0;
}
