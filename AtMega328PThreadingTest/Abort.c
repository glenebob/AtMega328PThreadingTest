#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "TestApplication.h"
#include "Error.h"
#include "Bits.h"
#include "UartDevice.h"
#include "File.h"

__ATTR_NORETURN__
void abort()
{
    exit(ERR_ABORT);
}

static void exit_safe(int) __ATTR_NORETURN__;

__ATTR_NORETURN__
void exit(int status)
{
    cli();

    asm volatile (
        "out %0, %A1 \n\t"
        "out %2, %B1 \n\t"
        :
        : "i" (_SFR_IO_ADDR(SPL)), "r" ((uint16_t) RAMEND), "i" (_SFR_IO_ADDR(SPH))
    );

    exit_safe(status);
}

__ATTR_NORETURN__
static void exit_safe(int status)
{
    static const __flash char string_exit[] = "Exit!";

    char status_message[sizeof(string_exit) - 1];

    memcpy_P(status_message, string_exit, sizeof(status_message));

    DDRB |= BIT5;
    PORTB &= ~BIT5;

    uart_crash_init(UART_BAUD, UART_FLAGS);
    stdio_crash_init((device_t*) uart_device);

    uint8_t significantBitCount = (uint8_t) (16 - __builtin_clz((uint16_t) status));

    uint32_t bit_delays[] = { 200000, 800000 };

    uint16_t bit_masks[] =
    {
        0b1000000000000000, 0b0100000000000000, 0b0010000000000000, 0b0001000000000000,
        0b0000100000000000, 0b0000010000000000, 0b0000001000000000, 0b0000000100000000,
        0b0000000010000000, 0b0000000001000000, 0b0000000000100000, 0b0000000000010000,
        0b0000000000001000, 0b0000000000000100, 0b0000000000000010, 0b0000000000000001
    };

    for (;;)
    {
        fprintf_P(stderr, PSTR("exit(%d/0x%04X)\n"), status, status);

        uint32_t bitDelay;

        PORTB |= BIT5;

        for (volatile uint32_t i = 0; i < bit_delays[0]; ++i);

        PORTB &= ~BIT5;

        for (volatile uint32_t i = 0; i < bit_delays[0]; ++i);

        for (uint8_t bit = 16 - significantBitCount; bit < 16; ++bit)
        {
            bitDelay = bit_delays[status & bit_masks[bit] ? 1 : 0];

            PORTB |= BIT5;

            for (volatile uint32_t i = 0; i < bitDelay; ++i);
            
            PORTB &= ~BIT5;

            for (volatile uint32_t i = 0; i < bit_delays[0]; ++i);
        }

        for (volatile uint32_t i = 0; i < bit_delays[1] * 2; ++i);
    }
}
