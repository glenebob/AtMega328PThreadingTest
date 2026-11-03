#include <stdint.h>
#include <stdlib.h>

#include <avr/interrupt.h>

#include "TestApplication.h"
#include "Types.h"
#include "Error.h"
#include "Stack.h"
#include "RingBuffer.h"
#include "Thread.h"
#include "Context.h"

#include "Task.h"

void *task_queue_create(void)
{
    thread_lock();

    void *task_queue_data = malloc(sizeof(task_queue_t));

    thread_unlock();

    if (!task_queue_data)
    {
        return task_queue_data;
    }

    task_queue_initialize(task_queue_data);

    return task_queue_data;
}

void task_queue_initialize(void *task_queue_data)
{
    task_queue_t *task_queue = (task_queue_t*) task_queue_data;

    stack_initialize(&task_queue->task_free_list.header, TASK_COUNT);
    ring_buffer_initialize(&task_queue->task_queue.header, TASK_COUNT);

    for (object_id_t task_id = TASK_COUNT - 1; task_id != OBJECT_ID_INVALID;--task_id)
    {
        stack_push(&task_queue->task_free_list, &task_id, sizeof(object_id_t));
    }

    task_queue->waiting_thread_id = OBJECT_ID_INVALID;
}

void task_queue_destroy(void *task_queue_data)
{
    thread_lock();

    free(task_queue_data);

    thread_unlock();
}

bool task_queue_push(void *task_queue_data, task_handler_t handler, void *arguments)
{
    task_queue_t *task_queue = (task_queue_t*) task_queue_data;

    cli();

    object_id_t task_id;

    if (!stack_pop(&task_queue->task_free_list, &task_id, sizeof(object_id_t)))
    {
        sei();

        return false;
    }

    task_queue->tasks[task_id].handler = handler;
    task_queue->tasks[task_id].arguments = arguments;

    ring_buffer_push(&task_queue->task_queue, &task_id, sizeof(object_id_t));

    if (task_queue->waiting_thread_id == OBJECT_ID_INVALID)
    {
        sei();

        return true;
    }

    thread_queue(task_queue->waiting_thread_id);

    task_queue->waiting_thread_id = OBJECT_ID_INVALID;

    if (!thread_select_if())
    {
        sei();

        return true;
    }

    thread_yield_dispatch();

    sei();

    return true;
}

void task_queue_pop(void *task_queue_data, task_handler_t *handler, void **arguments)
{
    task_queue_t *task_queue = (task_queue_t*) task_queue_data;

    cli();

    object_id_t task_id;

    if (ring_buffer_pop(&task_queue->task_queue, &task_id, sizeof(object_id_t)))
    {
        sei();

        *handler = task_queue->tasks[task_id].handler;
        *arguments = task_queue->tasks[task_id].arguments;

        stack_push(&task_queue->task_free_list, &task_id, sizeof(object_id_t));

        return;
    }

    task_queue->waiting_thread_id = current_thread_id;

    thread_block_and_select();
    thread_yield_dispatch();

    ring_buffer_pop(&task_queue->task_queue, &task_id, sizeof(object_id_t));

    *handler = task_queue->tasks[task_id].handler;
    *arguments = task_queue->tasks[task_id].arguments;

    stack_push(&task_queue->task_free_list, &task_id, sizeof(object_id_t));

    sei();
}
