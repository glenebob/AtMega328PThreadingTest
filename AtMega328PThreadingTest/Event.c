#include <stdint.h>
#include <stdlib.h>

#include <avr/interrupt.h>

#include "Types.h"
#include "Error.h"
#include "RingBuffer.h"
#include "Thread.h"
#include "Context.h"

#include "Event.h"

void *event_create(uint8_t flags)
{
    thread_lock();

    void *event_data = malloc(sizeof(event_t));

    thread_unlock();

    if (!event_data)
    {
        return event_data;
    }

    event_initialize(event_data, flags);
    
    return event_data;
}

void event_initialize(void *event_data, uint8_t flags)
{
    event_t *event = (event_t*) event_data;

    event->flags = flags;
    ring_buffer_initialize(&event->waiters.header, EVENT_WAIT_QUEUE_COUNT);
}

void event_destroy(void *event_data)
{
    thread_lock();

    free(event_data);

    thread_unlock();
}

bool event_wait(void *event_data)
{
    event_t *event = (event_t*) event_data;

    cli();

    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    if (event->flags & EVENT_TRIGGERED)
    {
        if (event->flags & EVENT_AUTO_RESET)
        {
            event->flags &= ~EVENT_TRIGGERED;
        }

        sei();

        return true;
    }

    if (!ring_buffer_push(&event->waiters, &current_thread_id, sizeof(current_thread_id)))
    {
        sei();

        return false;
    }

    thread_block();
    thread_yield_dispatch();

    sei();

    return true;
}

void event_trigger(void *event_data)
{
    event_t *event = (event_t*) event_data;

    cli();

    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    if (event->flags & EVENT_TRIGGERED)
    {
        sei();
        
        return;
    }

    object_id_t thread_id;

    if (!ring_buffer_pop(&event->waiters, &thread_id, sizeof(object_id_t)))
    {
        sei();
        
        return;
    }
                
    if (event->flags & EVENT_AUTO_RESET)
    {
        event->flags &= ~EVENT_TRIGGERED;
    }
        
    thread_queue(thread_id);

    while (ring_buffer_pop(&event->waiters, &thread_id, sizeof(object_id_t)))
    {
        thread_queue(thread_id);
    }    

    if (thread_select())
    {
        thread_block();

        SAVE_FIRST_HALF_CONTEXT();
        SAVE_SECOND_HALF_CONTEXT();
        SAVE_STACK_POINTER();

        thread_change();

        JUMP(thread_restore_all_dispatch);
    }

    sei();

    return;
}
