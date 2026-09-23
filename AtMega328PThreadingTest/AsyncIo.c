#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "F_CPU.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

#include "Types.h"
#include "Error.h"
#include "Interrupt.h"
#include "Timer.h"
#include "Thread.h"
#include "Assembly.h"

#include "AsyncIo.h"

//#define BAUD 9600
#define BAUD 62500
#define BAUDRATE ((F_CPU) / (BAUD * 16UL) -1)

typedef struct
{
    bool active;
    uint8_t *buffer;
    uint8_t buffer_length;
    uint8_t offset;
    uint8_t timer_id;
    uint8_t thread_id;
} io_task_t;

static io_task_t read_task;
static io_task_t write_task;

static int uart_getchar(FILE *stream);
static int uart_putchar(char c, FILE *stream);

static FILE uart_stdin;
static FILE uart_stdout_err;

static uint16_t getchar_timeout;

void io_task_init()
{
    read_task.active = false;
    read_task.thread_id = 0xFF;

    write_task.active = false;
    write_task.thread_id = 0xFF;

    UBRR0H = (uint8_t) (BAUDRATE >> 8);
    UBRR0L = (uint8_t) BAUDRATE;
    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    // Set frame format: 8data, 2stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXCIE0) | (1 << TXEN0);
    
    fdev_setup_stream(&uart_stdin, NULL, uart_getchar, _FDEV_SETUP_READ);
    fdev_setup_stream(&uart_stdout_err, uart_putchar, NULL, _FDEV_SETUP_WRITE);
    
    stdin = &uart_stdin;
    stdout = &uart_stdout_err;

    getchar_timeout = 0;
};

void io_crash_init(int(*put)(char, struct __file *))
{
    fdev_setup_stream(&uart_stdout_err, put, NULL, _FDEV_SETUP_WRITE);
}

static int io_dispatch(io_task_t *io_task, uint16_t timeout)
{
    io_task->active = true;
    io_task->thread_id = current_thread_id;

    if (timeout > 0)
    {
        io_task->timer_id = timer_add(current_thread_id, timeout);
    }
    else
    {
        io_task->timer_id = OBJECT_ID_INVALID;
    }

    thread_block(current_thread_id);
    thread_yield_dispatch();

    int return_value = io_task->offset < io_task->buffer_length ? -1 : io_task->buffer_length;

    io_task->active = false;
    io_task->thread_id = OBJECT_ID_INVALID;

    return return_value;
}

int io_read(uint8_t *buffer, uint8_t buffer_length, uint16_t timeout)
{
    INTERRUPT_RAISE_LEVEL();

    if (read_task.active || write_task.active)
    {
        exit(ERR_IO_READ_DUP);
    }

    read_task.offset = 0;

    if (UCSR0A & (1 << RXC0))
    {
        buffer[read_task.offset++] = UDR0;
        
        if (read_task.offset == buffer_length)
        {
            INTERRUPT_LOWER_LEVEL();
            
            return buffer_length;
        }
    }

    read_task.buffer = buffer;
    read_task.buffer_length = buffer_length;

    return io_dispatch(&read_task, timeout);
}

int io_write(const uint8_t *buffer, uint8_t buffer_length, uint16_t timeout)
{
    INTERRUPT_RAISE_LEVEL();

    if (read_task.active || write_task.active)
    {
        exit(ERR_IO_WRITE_DUP);
    }

    write_task.offset = 0;

    if (UCSR0A & (1 << UDRE0))
    {
        UDR0 = buffer[write_task.offset++];
        
        if (write_task.offset == buffer_length)
        {
            INTERRUPT_LOWER_LEVEL();
            
            return buffer_length;
        }
    }

    write_task.buffer = (uint8_t*) buffer;
    write_task.buffer_length = buffer_length;

    return io_dispatch(&write_task, timeout);
}

static int uart_getchar(FILE *stream)
{
    char c;
    
    int result = io_read((uint8_t*) &c, 1, getchar_timeout);
    
    if (result == -1)
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

static int uart_putchar(char c, FILE *stream)
{
    if (c == '\n')
    {
        char buffer[2];

        buffer[0] = '\r';
        buffer[1] = c;
        
        io_write((uint8_t*) buffer, 2, 0);
    }
    else
    {
        io_write((uint8_t*) &c, 1, 0);
    }

    return 0;
}

void set_getchar_timeout(uint16_t timeout)
{
    getchar_timeout = timeout;
}

ISR(USART_RX_vect)
{
    // Always clear the USART buffer regardless of where the byte might end up.
    uint8_t b = UDR0;
    
    if (!read_task.active)
    {
        return;
    }

    read_task.buffer[read_task.offset++] = b;

    if (read_task.offset != read_task.buffer_length)
    {
        return;
    }

    read_task.active = false;

    if (read_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(read_task.timer_id);
    }

    thread_ensure_runnable(read_task.thread_id);
}

__attribute__((naked))
ISR(USART_TX_vect)
{
    SAVE_FIRST_HALF_CONTEXT();

    if (!write_task.active)
    {
        goto return_no_context_switch;
    }

    UDR0 = write_task.buffer[write_task.offset++];
        
    if (write_task.offset != write_task.buffer_length)
    {
        goto return_no_context_switch;
    }

    write_task.active = false;

    if (write_task.timer_id != OBJECT_ID_INVALID)
    {
        timer_cancel(write_task.timer_id);
    }

    thread_ensure_runnable(write_task.thread_id);

    if (current_thread_id != THREAD_ID_IDLE)
    {
        goto return_no_context_switch;
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_set_current(write_task.thread_id);

    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();
    RESTORE_FIRST_HALF_CONTEXT();
    DISPATCH();

return_no_context_switch:
    RESTORE_FIRST_HALF_CONTEXT();
    DISPATCH();
}
