#include <stdint.h>
#include <stdlib.h>

#include "Error.h"

uint8_t interrupt_level;

void interrupt_initialize()
{
    interrupt_level = 1;
}
