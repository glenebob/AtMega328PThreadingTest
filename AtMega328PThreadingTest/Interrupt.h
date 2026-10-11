#pragma once

#include <stdlib.h>

#include <avr/interrupt.h>

#include "Error.h"

extern uint8_t interrupt_level;

void interrupt_initialize();

#define INTERRUPT_RAISE_LEVEL() cli(); if (interrupt_level++ == 0xFF) exit(ERR_INT_RAISE_BAD_LEVEL)
#define INTERRUPT_LOWER_LEVEL() if (interrupt_level == 0) exit(ERR_INT_LOWER_BAD_LEVEL); if (--interrupt_level == 0) sei()

#define INTERRUPT_LOWER_LEVEL_NO_ENABLE() \
    asm volatile ( \
        "push r0                    \n\t" \
        "in r0, __SREG__            \n\t" \
        "push r0                    \n\t" \
        "push r1                    \n\t" \
        "push r24                   \n\t" \
        "push r25                   \n\t" \
        "clr r1                     \n\t" \
        "lds r24, interrupt_level   \n\t" \
        "cpse r24, r1               \n\t" \
        "rjmp 1f                    \n\t" \
        "ldi r24, %[BAD_LEVEL]      \n\t" \
        "ldi r25, 0x00              \n\t" \
        "call exit                  \n\t" \
        "1:                         \n\t" \
        "subi r24, 0x01             \n\t" \
        "sts interrupt_level, r24   \n\t" \
        "pop r25                    \n\t" \
        "pop r24                    \n\t" \
        "pop r1                     \n\t" \
        "pop r0                     \n\t" \
        "out __SREG__, r0           \n\t" \
        "pop r0                     \n\t" \
        :: [BAD_LEVEL] "i" (ERR_INT_LOWER_BAD_LEVEL) \
    )
