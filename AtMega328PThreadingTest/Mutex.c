#include <stdint.h>
#include <stdlib.h>

#include "Types.h"
#include "RingBuffer.h"
#include "Thread.h"
#include "Interrupt.h"

#include "Mutex.h"

void *mutex_create()
{
    void *mutex_data = malloc(sizeof(mutex_t));

    if (!mutex_data)
    {
        return mutex_data;
    }

    mutex_initialize(mutex_data);
    
    return mutex_data;
}

void mutex_initialize(void *mutex_data)
{
    mutex_t *mutex = (mutex_t*) mutex_data;

    mutex->thread_id = OBJECT_ID_INVALID;
    ring_buffer_initialize(&mutex->waiters.header, MUTEX_WAIT_QUEUE_COUNT);
}

void mutex_destroy(void *mutex_data)
{
    free(mutex_data);
}

bool mutex_lock(void *mutex_data)
{
    mutex_t *mutex = (mutex_t*) mutex_data;

    INTERRUPT_RAISE_LEVEL();

    if (mutex->thread_id == OBJECT_ID_INVALID)
    {
        mutex->thread_id = current_thread_id;

        INTERRUPT_LOWER_LEVEL();
        
        return true;
    }

    if (!ring_buffer_push(&mutex->waiters, &current_thread_id, sizeof(current_thread_id)))
    {
        INTERRUPT_LOWER_LEVEL();

        return false;
    }

    thread_block(current_thread_id);
    thread_yield_dispatch();

    return true;
}

bool mutex_unlock(void *mutex_data)
{
    mutex_t *mutex = (mutex_t*) mutex_data;

    INTERRUPT_RAISE_LEVEL();

    if (mutex->thread_id != current_thread_id)
    {
        INTERRUPT_LOWER_LEVEL();

        return false;
    }

    if (!ring_buffer_pop(&mutex->waiters, &mutex->thread_id, sizeof(object_id_t)))
    {
        mutex->thread_id = OBJECT_ID_INVALID;

        INTERRUPT_LOWER_LEVEL();

        return true;
    }

    thread_queue(mutex->thread_id);
    thread_yield_dispatch();

    return true;
}
