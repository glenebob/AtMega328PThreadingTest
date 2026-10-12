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

static const __flash char crash_report_format[] PROGMEM = "exit(%d/0x%04X)\n";

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

    uint8_t significantBitCount = 1;

    for (uint8_t shift = 15; shift != 0xFF; --shift)
    {
        if ((unsigned int) status >> shift)
        {
            significantBitCount += shift;
            break;
        }
    }
    
    for (;;)
    {
        printf_P(crash_report_format, status, status);

        uint32_t bitDelay;

        bitDelay = bit_delays[0];

        PORTB |= (1 << PB5);

        for (volatile uint32_t i = 0; i < bitDelay; ++i);

        PORTB &= ~(1 << PB5);

        for (volatile uint32_t i = 0; i < 200000; ++i);

        for (uint8_t shift = significantBitCount - 1; shift != 0xFF; --shift)
        {
            bitDelay = bit_delays[(((unsigned int) status >> shift) & 0b00000001)];

            PORTB |= (1 << PB5);

            for (volatile uint32_t i = 0; i < bitDelay; ++i);
            
            PORTB &= ~(1 << PB5);

            for (volatile uint32_t i = 0; i < 200000; ++i);
        }

        for (volatile uint32_t i = 0; i < 2000000; ++i);
    }
}

static void io_write_spin(uint8_t b)
{
    while (!(UCSR0A & (1 << UDRE0)));

    UDR0 = b;
}

static int uart_putchar_spin(char c, FILE *stream)
{
    if (c == '\n')
    {
        io_write_spin('\r');
    }

    io_write_spin(c);

    return 0;
}
