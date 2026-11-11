#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Types.h"
#include "Thread.h"

#include "Stack.h"

typedef struct
{
    stack_header_t header;
    uint8_t data[];
} stack_t;

void *stack_create(uint8_t item_count, uint8_t data_size)
{
    thread_lock();

    void *stack_data = malloc(sizeof(stack_t) + item_count * data_size);

    thread_unlock();

    if (!stack_data)
    {
        return stack_data;
    }

    stack_initialize(stack_data, item_count);

    return stack_data;
}

void stack_initialize(stack_header_t *header, uint8_t item_count)
{
    header->last = item_count - 1;
    header->top = 0xFF;
}

void stack_destroy(void *stack_data)
{
    thread_lock();

    free(stack_data);

    thread_unlock();
}

bool stack_push(void *stack_data, void *data, uint8_t data_size)
{
    stack_t *stack = (stack_t*) stack_data;

    if (stack->header.top == stack->header.last)
    {
        return false;
    }

    ++stack->header.top;

    if (data_size == 1)
    {
        memcpy(&stack->data[stack->header.top], data, 1);
    }
    else
    {
        memcpy(&stack->data[stack->header.top * data_size], data, data_size);
    }

    return true;
}

bool stack_pop(void *stack_data, void *data, uint8_t data_size)
{
    stack_t *stack = (stack_t*) stack_data;

    if (stack->header.top == 0xFF)
    {
        return false;
    }

    if (data_size == 1)
    {
        memcpy(data, &stack->data[stack->header.top], 1);
    }
    else
    {
        memcpy(data, &stack->data[stack->header.top * data_size], data_size);
    }

    --stack->header.top;

    return true;
}
