#include <stdint.h>
#include <stdlib.h>

#include <avr/interrupt.h>

#include "Types.h"
#include "Error.h"
#include "Thread.h"
#include "Stack.h"
#include "LinkedList.h"

#define TIMER_COUNT THREAD_USER_COUNT

typedef struct
{
    uint8_t thread_id;
    uint16_t ticks;
} timer_t;

typedef struct
{
    stack_header_t header;
    object_id_t timers[TIMER_COUNT];
} timer_free_list_t;

static timer_t timers[TIMER_COUNT];
static timer_free_list_t timer_free_list;
static double_linked_list_node_t timer_nodes[TIMER_COUNT];
static double_linked_list_t timer_list;

void timers_initialize(void)
{
    stack_initialize(&timer_free_list.header, TIMER_COUNT);
    dl_initialize(&timer_list, &timer_nodes[0], TIMER_COUNT);

    for (object_id_t timer_id = TIMER_COUNT - 1; timer_id != OBJECT_ID_INVALID; --timer_id)
    {
        stack_push(&timer_free_list, &timer_id, sizeof(object_id_t));
    }

    TCCR0A = (1 << WGM01);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);
}

uint8_t timer_add(uint8_t thread_id, uint16_t ticks)
{
    object_id_t new_timer_id;

    if (!stack_pop(&timer_free_list, &new_timer_id, sizeof(object_id_t)))
    {
        return new_timer_id;
    }

    timer_t *timer = &timers[new_timer_id];

    timer->thread_id = thread_id;
    timer->ticks = ticks;

    for (uint8_t timer_id = timer_list.first_linked; timer_id != 0xFF; timer_id = timer_nodes[timer_id].next)
    {
        timer = &timers[timer_id];

        if (timer->ticks >= ticks)
        {
            dl_insert(&timer_list, new_timer_id, timer_id);

            return new_timer_id;
        }
    }

    dl_append(&timer_list, new_timer_id);

    return new_timer_id;
}

void timer_cancel(uint8_t timer_id)
{
    dl_remove(&timer_list, timer_id);
    stack_push(&timer_free_list, &timer_id, sizeof(object_id_t));
}

// Must be called with interrupts disabled.
// Only called from a timer ISR.
void advance_active_timers(void)
{
    for (uint8_t timer_id = timer_list.first_linked, next_timer_id; timer_id != OBJECT_ID_INVALID; timer_id = next_timer_id)
    {
        next_timer_id = timer_nodes[timer_id].next;

        timer_t *timer = &timers[timer_id];

        if (!--timer->ticks)
        {
            thread_queue(timer->thread_id);
            timer_cancel(timer_id);
        }
        else
        {
            // Beyond this point in the list, no timers can possibly be elapsed.
            // Decrement the rest, but no need to check for zero.
            for (timer_id = next_timer_id; timer_id != OBJECT_ID_INVALID; timer_id = timer_nodes[timer_id].next)
            {
                --timers[timer_id].ticks;
            }

            break;
        }
    }
}
