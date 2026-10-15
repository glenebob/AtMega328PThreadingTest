#include <stdint.h>
#include <string.h>

#include <avr/pgmspace.h>

#include "Types.h"
#include "Thread.h"
#include "Context.h"

extern uint8_t _end;

__attribute__((OS_main))
int main(void)
{
    for (uint8_t *address = &_end; address <= 0x08FD; ++address)
    {
        *address = 0;
    }

    static const __flash char string_main[] PROGMEM = "Main!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_main) - 1];

    memcpy_P(status, string_main, sizeof(status));

    threads_initialize();

    JUMP(thread_kernel_entry);
}
