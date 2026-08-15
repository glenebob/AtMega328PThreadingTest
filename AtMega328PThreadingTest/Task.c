#include <stdint.h>
#include <stdlib.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Types.h"
#include "Error.h"
#include "Interrupt.h"
#include "Task.h"

#define TASK_COUNT 5

struct
{
    uint8_t last;
    uint8_t next_in;
    uint8_t next_out;
    struct
    {
        task_handler_t handler;
        void *arguments;
    } tasks[TASK_COUNT];
} task_queue;

void task_queue_init()
{
    task_queue.last = TASK_COUNT - 1;
    task_queue.next_in = 0;
    task_queue.next_out = 0xFF;
}

void task_queue_push(task_handler_t handler, void *arguments)
{
    INTERRUPT_RAISE_LEVEL();

    if (task_queue.next_in == task_queue.next_out)
    {
        exit(ERR_TASK_QUEUE_FULL);
    }

    task_queue.tasks[task_queue.next_in].handler = handler;
    task_queue.tasks[task_queue.next_in].arguments = arguments;

    if (task_queue.next_out == 0xFF)
    {
        task_queue.next_out = task_queue.next_in;
    }

    if (task_queue.next_in == task_queue.last)
    {
        task_queue.next_in = 0;
    }
    else
    {
        ++task_queue.next_in;
    }

    INTERRUPT_LOWER_LEVEL();
}

bool task_queue_pop(task_handler_t *handler, void **arguments)
{
    INTERRUPT_RAISE_LEVEL();

    if (task_queue.next_out == 0xFF)
    {
        INTERRUPT_LOWER_LEVEL();

        return false;
    }

    *handler = task_queue.tasks[task_queue.next_out].handler;
    *arguments = task_queue.tasks[task_queue.next_out].arguments;

    ++task_queue.next_out;

    if (task_queue.next_out > task_queue.last)
    {
        task_queue.next_out = 0;
    }

    if (task_queue.next_out == task_queue.next_in)
    {
        task_queue.next_out = 0xFF;
    }

    INTERRUPT_LOWER_LEVEL();

    return true;
}
