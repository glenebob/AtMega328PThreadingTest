#include <stdint.h>
#include <string.h>

#include "Types.h"

#include "Stack.h"

typedef struct
{
    stack_header_t header;
    uint8_t data[1];
} stack_t;

void stack_initialize(stack_header_t *header, uint8_t item_count)
{
    header->last = item_count - 1;
    header->top = 0xFF;
}

bool stack_push(void *stack_data, void *data, uint8_t data_size)
{
    stack_t *stack = (stack_t*) stack_data;

    if (stack->header.top == stack->header.last)
    {
        return false;
    }

    ++stack->header.top;
    memcpy(&stack->data[stack->header.top * data_size], data, data_size);

    return true;
}

bool stack_pop(void *stack_data, void *data, uint8_t data_size)
{
    stack_t *stack = (stack_t*) stack_data;

    if (stack->header.top == 0xFF)
    {
        return false;
    }

    memcpy(data, &stack->data[stack->header.top * data_size], data_size);
    --stack->header.top;

    return true;
}
