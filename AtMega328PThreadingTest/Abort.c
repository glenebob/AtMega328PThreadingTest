#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "Error.h"
#include "AsyncIo.h"

static int uart_putchar_spin(char c, FILE *stream);

static const uint32_t bit_delays[] = { 200000, 800000 };
const __flash char crash_report_format[] PROGMEM = "exit(%d/0x%X)\n";

void abort()
{
    exit(ERR_ABORT);
}

void exit(int status)
{
    cli();

    DDRB |= (1 << PB5);
    PORTB &= ~(1 << PB5);

    io_crash_init(uart_putchar_spin);

    uint8_t significantBitCount = 2;
    
    for (int8_t shift = 7; shift >= 0; --shift)
    {
        if (status >> shift)
        {
            significantBitCount += shift;
            break;
        }
    }
    
    for (;;)
    {
        printf_P(crash_report_format, status, status);

        for (int8_t shift = significantBitCount - 1; shift >= 0; --shift)
        {
            uint32_t bitDelay = bit_delays[((status >> shift) & 0b00000001)];

            PORTB |= (1 << PB5);

            for (uint32_t i = 0; i < bitDelay; ++i);
            
            PORTB &= ~(1 << PB5);

            for (uint32_t i = 0; i < 200000; ++i);
        }

        for (uint32_t i = 0; i < 2000000; ++i);
    }
}

static void io_write_spin(uint8_t *buffer, uint8_t buffer_length)
{
    while (buffer_length--)
    {
        while (!(UCSR0A & (1 << UDRE0)));

        UDR0 = *buffer++;
    }
}

static int uart_putchar_spin(char c, FILE *stream)
{
    if (c == '\n')
    {
        char buffer[2];

        buffer[0] = '\r';
        buffer[1] = c;

        io_write_spin((uint8_t*) buffer, 2);
    }
    else
    {
        io_write_spin((uint8_t*) &c, 1);
    }

    return 0;
}
