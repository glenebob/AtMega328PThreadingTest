#include <stdint.h>
#include <stdlib.h>

#include "Types.h"
#include "Error.h"
#include "Interrupt.h"
#include "Stack.h"
#include "RingBuffer.h"

#include "Task.h"

#define TASK_COUNT 5

typedef struct
{
    task_handler_t handler;
    void *arguments;
} task_t;

typedef struct
{
    stack_header_t header;
    object_id_t tasks[TASK_COUNT];
} task_free_list_t;

typedef struct
{
    ring_buffer_header_t header;
    object_id_t tasks[TASK_COUNT];
} task_queue_t;

static task_t tasks[TASK_COUNT];
static task_free_list_t task_free_list;
static task_queue_t task_queue;

void task_queue_initialize()
{
    stack_initialize(&task_free_list.header, TASK_COUNT);
    ring_buffer_initialize(&task_queue.header, TASK_COUNT);

    for (object_id_t task_id = TASK_COUNT - 1; task_id != OBJECT_ID_INVALID;--task_id)
    {
        stack_push(&task_free_list, &task_id, sizeof(object_id_t));
    }
}

bool task_queue_push(task_handler_t handler, void *arguments)
{
    INTERRUPT_RAISE_LEVEL();

    object_id_t task_id;

    if (!stack_pop(&task_free_list, &task_id, sizeof(object_id_t)))
    {
        INTERRUPT_LOWER_LEVEL();

        return false;
    }

    tasks[task_id].handler = handler;
    tasks[task_id].arguments = arguments;

    ring_buffer_push(&task_queue, &task_id, sizeof(object_id_t));

    INTERRUPT_LOWER_LEVEL();

    return true;
}

bool task_queue_pop(task_handler_t *handler, void **arguments)
{
    INTERRUPT_RAISE_LEVEL();

    object_id_t task_id;

    if (!ring_buffer_pop(&task_queue, &task_id, sizeof(object_id_t)))
    {
        INTERRUPT_LOWER_LEVEL();

        return false;
    }

    *handler = tasks[task_id].handler;
    *arguments = tasks[task_id].arguments;

    stack_push(&task_free_list, &task_id, sizeof(object_id_t));

    INTERRUPT_LOWER_LEVEL();

    return true;
}
