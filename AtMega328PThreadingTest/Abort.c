#include <stdint.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "Error.h"

static const uint32_t bit_delays[] = { 200000, 800000 };

void abort()
{
    exit(ERR_CLIB_ABORT);
}

void exit(int status)
{
    cli();

    DDRB |= (1 << PB5);
    PORTB &= ~(1 << PB5);
    
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
