#include <stdint.h>
#include <stdlib.h>

#include <avr/interrupt.h>

#include "Types.h"
#include "Error.h"
#include "RingBuffer.h"
#include "Thread.h"

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

    cli();

    if (mutex->thread_id == OBJECT_ID_INVALID)
    {
        mutex->thread_id = current_thread_id;
        mutex->use_count = 1;

        sei();

        return true;
    }
    else if (mutex->thread_id == current_thread_id)
    {
        ++mutex->use_count;

        if (mutex->use_count == 0xFF)
        {
            exit(ERR_MUTEX_USE_COUNT_OVERFLOW);
        }

        sei();
        
        return true;
    }

    if (!ring_buffer_push(&mutex->waiters, &current_thread_id, sizeof(current_thread_id)))
    {
        sei();

        return false;
    }

    thread_block();
    thread_yield_dispatch();

    sei();

    return true;
}

bool mutex_unlock(void *mutex_data)
{
    mutex_t *mutex = (mutex_t*) mutex_data;

    cli();

    if (mutex->thread_id != current_thread_id)
    {
        sei();

        return false;
    }

    if (--mutex->use_count)
    {
        sei();

        return true;
    }

    mutex->thread_id = OBJECT_ID_INVALID;

    if (!ring_buffer_pop(&mutex->waiters, &mutex->thread_id, sizeof(object_id_t)))
    {
        mutex->thread_id = OBJECT_ID_INVALID;

        sei();

        return true;
    }

    mutex->use_count = 1;

    thread_queue(mutex->thread_id);

    object_id_t new_thread_id;

    if (!thread_select(&new_thread_id))
    {
        sei();

        return true;
    }

    thread_change(new_thread_id);
    thread_yield_dispatch();

    sei();

    return true;
}
