#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "TestApplication.h"
#include "Types.h"
#include "Error.h"
#include "Stack.h"
#include "RingBuffer.h"
#include "Timer.h"
#include "Context.h"
#include "Task.h"

#include "Thread.h"

#define STACK_SIZE_KERNEL 256
#define STACK_SIZE_IDLE 64
#define STACK_SIZE_USER 256

#define THREAD_USER_TICKS 10

#define PRIORITY_COUNT 2

typedef struct
{
    stack_header_t header;
    object_id_t threads[THREAD_USER_COUNT];
} thread_free_list_t;

typedef struct
{
    ring_buffer_header_t header;
    object_id_t thread_id[THREAD_USER_COUNT];
} thread_run_queue_t;

static thread_t threads[THREAD_COUNT];
static thread_free_list_t thread_free_list;
static thread_run_queue_t run_queues[PRIORITY_COUNT];

uint8_t current_thread_id;
thread_t *current_thread;
uint8_t current_thread_priority;
uint8_t thread_lock_count;

volatile uint32_t idle_spin_count = 0;

static void global_thread_entry(void);
static void thread_idle_entry(void*);

extern uint8_t __data_start;
extern uint8_t _end;

uint16_t malloc_heap_size;

static object_id_t new_thread_id;

static void initialize_thread_stack(object_id_t thread_id, thread_t *thread);

int main(void);

void threads_initialize()
{
    // Sanity check to make sure the total stack space does not crash into
    // the data segments.
    uint16_t total_data_bytes = (uint16_t) &_end - (uint16_t ) &__data_start;
    uint16_t total_stacks_size = STACK_SIZE_KERNEL + STACK_SIZE_IDLE + ((THREAD_COUNT - 2) * STACK_SIZE_USER);
    int16_t heap_size = 2048 - total_data_bytes - total_stacks_size;

    if (malloc_heap_size < 0)
    {
        exit(ERR_THREAD_STACK_HEAP_COLLISION);
    }

    malloc_heap_size = heap_size;

    // Sandbox malloc to the region below the total stack space.
    // Since the stack pointer is completely unstable and we know
    // total stack size, we can set malloc's margin to zero.
    __malloc_heap_end = (char*) (__malloc_heap_start + malloc_heap_size);
    __malloc_margin = 0;

    uint8_t thread_id = 0;
    thread_t *thread = &threads[thread_id];

    // Kernel thread - currently running
    thread->status = THREAD_STATUS_RUNNABLE;
    thread->entry_point = (thread_entry_t) main;
    thread->argument = NULL;
    thread->ticks = 0xFF;

    ++thread_id;
    thread = &threads[thread_id];

    // Idle thread
    initialize_thread_stack(thread_id, thread);

    thread->status = THREAD_STATUS_RUNNABLE;
    thread->entry_point = thread_idle_entry;
    thread->argument = NULL;
    thread->ticks = 0xFF;

    // User threads
    for (++thread_id; thread_id < THREAD_COUNT; ++thread_id)
    {
        threads[thread_id].status = THREAD_STATUS_DORMANT;
    }

    stack_initialize(&thread_free_list.header, THREAD_USER_COUNT);

    for (int i = 0; i < PRIORITY_COUNT; ++i)
    {
        ring_buffer_initialize(&run_queues[i].header, THREAD_USER_COUNT);
    }

    for (object_id_t thread_id = THREAD_COUNT - 1; thread_id >= THREAD_ID_FIRST_USER; --thread_id)
    {
        threads[thread_id].status = THREAD_STATUS_DORMANT;
        stack_push(&thread_free_list, &thread_id, sizeof(object_id_t));
    }

    current_thread_id = THREAD_ID_KERNEL;
    current_thread_priority = 0;
    current_thread = &threads[current_thread_id];
    thread_lock_count = 0;
}

static void initialize_thread_stack(object_id_t thread_id, thread_t *thread)
{
    typedef struct
    {
        uint8_t r1_register;
        uint8_t status_register;
        uint8_t r0_register_placeholder_not_used;
        struct
        {
            uint8_t high;
            uint8_t low;
        } return_address;
    } stack_init_block_t;

    stack_init_block_t *stack_init_block;

    if (thread_id == THREAD_ID_IDLE)
    {
        stack_init_block = (stack_init_block_t*) (0x8FF - STACK_SIZE_KERNEL - sizeof(stack_init_block_t) + 1);
    }
    else
    {
        stack_init_block = (stack_init_block_t*) (0x8FF - STACK_SIZE_KERNEL - STACK_SIZE_IDLE - (thread_id - 2) * STACK_SIZE_USER - sizeof(stack_init_block_t) + 1);
    }

    stack_init_block->r1_register = 0;
    stack_init_block->status_register = 0; //0x80;
    stack_init_block->return_address.high = (uint16_t) global_thread_entry >> 8;
    stack_init_block->return_address.low = (uint16_t) global_thread_entry & 0xFF;

    thread->saved_stack_pointer = (uint16_t) (((void*) stack_init_block) - sizeof(uint16_t*) - 29);
}

object_id_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable)
{
    cli();

    object_id_t thread_id = OBJECT_ID_INVALID;

    stack_pop(&thread_free_list, &thread_id, sizeof(object_id_t));

    sei();

    if (thread_id == OBJECT_ID_INVALID)
    {
        return thread_id;
    }

    thread_t *thread = &threads[thread_id];

    initialize_thread_stack(thread_id, thread);

    thread->status = THREAD_STATUS_BLOCKED;
    thread->entry_point = entry_point;
    thread->argument = argument;
    thread->ticks = preemptable ? 0 : 0xFF;
    
    cli();

    thread_queue(thread_id);

    sei();

    return thread_id;
}

void thread_lock(void)
{
    cli();

    if (!++thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    sei();
}

void thread_unlock(void)
{
    cli();

    if (!thread_lock_count--)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    if ((thread_lock_count || current_thread->ticks > 0) && thread_select())
    {
        SAVE_FIRST_HALF_CONTEXT_ISR();
        SAVE_SECOND_HALF_CONTEXT();
        SAVE_STACK_POINTER();

        thread_change();

        JUMP(thread_restore_all_dispatch);
    }

    sei();
}

void thread_block()
{
    if (current_thread_priority == 0)
    {
        threads[THREAD_ID_KERNEL].status = THREAD_STATUS_BLOCKED;
    }
    else
    {
        object_id_t thread_id;

        ring_buffer_pop(&run_queues[current_thread_priority - 1], &thread_id, sizeof(object_id_t));
        threads[thread_id].status = THREAD_STATUS_BLOCKED;
    }
}

void thread_kernel_dormant()
{
    threads[THREAD_ID_KERNEL].status = THREAD_STATUS_DORMANT;
}

// Must be called with interrupts disabled.
void thread_queue(object_id_t thread_id)
{
    thread_t *thread = &threads[thread_id];

    thread->status = THREAD_STATUS_RUNNABLE;

    if (thread_id != THREAD_ID_KERNEL)
    {
        ring_buffer_push(&run_queues[0], &thread_id, sizeof(object_id_t));
    }
}

void thread_requeue()
{
    switch (current_thread_priority)
    {
    case 1:
        {
            object_id_t thread_id;

            ring_buffer_pop(&run_queues[0], &thread_id, sizeof(object_id_t));
            ring_buffer_push(&run_queues[1], &thread_id, sizeof(object_id_t));
        }

        break;
    case 2:
        ring_buffer_rotate(&run_queues[1], sizeof(object_id_t));

        break;
    }
}

void thread_destroy(object_id_t thread_id)
{
    cli();

    threads[thread_id].status = THREAD_STATUS_DORMANT;
    stack_push(&thread_free_list, &thread_id, sizeof(object_id_t));

    sei();
}

void thread_sleep(uint16_t milliseconds)
{
	if (!milliseconds)
	{
		return;
	}

	cli();

    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

	timer_add(current_thread_id, milliseconds);
    thread_block();
    thread_yield_dispatch();

    sei();
}

bool thread_select(void)
{
    new_thread_id = THREAD_ID_IDLE;
    current_thread_priority = 3;

    if (threads[THREAD_ID_KERNEL].status == THREAD_STATUS_RUNNABLE)
    {
        new_thread_id = THREAD_ID_KERNEL;
        current_thread_priority = 0;
    }
    else if (threads[THREAD_ID_KERNEL].status == THREAD_STATUS_DORMANT)
    {
        for (uint8_t i = 0; i < PRIORITY_COUNT; ++i)
        {
            object_id_t thread_id;

            if (ring_buffer_peek(&run_queues[i], &thread_id, sizeof(object_id_t)))
            {
                new_thread_id = thread_id;
                current_thread_priority = i + 1;

                break;
            }
        }
    }

    return !thread_lock_count && new_thread_id != current_thread_id;
}

void thread_select_and_change(void)
{
    if (thread_select())
    {
        thread_change();
    }
}

void thread_change(void)
{
    current_thread_id = new_thread_id;
    current_thread = &threads[current_thread_id];

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = THREAD_USER_TICKS;
    }
}

__attribute__((naked))
static void global_thread_entry(void)
{
    sei();

    current_thread->entry_point(current_thread->argument);

    cli();

    thread_block();
    thread_destroy(current_thread_id);

    thread_select_and_change();

    JUMP(thread_restore_all_dispatch);
}

void thread_root_entry(void*);

void kernel_init_task(void *unused_arguments)
{
    timers_initialize();
    thread_create(thread_root_entry, NULL, true);
}

__attribute__((OS_main))
void thread_kernel_entry(void *unused_arguments)
{
    static const __flash char string_kernel[] PROGMEM = "Kernel!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_kernel) - 1];

    memcpy_P(status, string_kernel, sizeof(status));

    threads_initialize();
    task_queue_initialize();

    // Queue up remaining initialization.
    task_queue_push(kernel_init_task, NULL);

    sei();

    while (trueFlag)
    {
        task_handler_t handler;
        void *arguments;

        while (task_queue_pop(&handler, &arguments))
        {
            static const __flash char string_kernel_task[] PROGMEM = "Kernel Task!";

            char status[sizeof(string_kernel_task) - 1];

            memcpy_P(status, string_kernel_task, sizeof(status));

            handler(arguments);

            memset(status, 0, sizeof(status));
        }

        cli();

        thread_kernel_dormant();
        thread_yield_dispatch();

        sei();
    }

    memset(status, 0, sizeof(status));

    abort();
}

__attribute__((OS_task))
static void thread_idle_entry(void *unused_arguments)
{
    static const __flash char string_idle[] PROGMEM = "Idle!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_idle) - 1];

    memcpy_P(status, string_idle, sizeof(status));

    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    while (trueFlag)
    {
        cli();
        ++idle_spin_count;
        sei();
        sleep_cpu();
    }

    memset(status, 0, sizeof(status));
}

__attribute__((naked))
void thread_yield_dispatch()
{
    SAVE_FIRST_HALF_CONTEXT();
    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_select_and_change();

    JUMP(thread_restore_all_dispatch);
}

void thread_post_kernel_task(task_handler_t handler, void *arguments)
{
    cli();

    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    task_queue_push(handler, arguments);
    thread_queue(THREAD_ID_KERNEL);
    thread_yield_dispatch();

    sei();
}

// Must be jumped to
__attribute__((naked))
void thread_restore_first_half_dispatch()
{
    RESTORE_FIRST_HALF_CONTEXT();
    RETURN();
}

// Must be jumped to
__attribute__((naked))
void thread_restore_all_dispatch()
{
    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();

    JUMP(thread_restore_first_half_dispatch);
}

void advance_thread_ticks(void)
{
    if (current_thread->ticks != 0xFF)
    {
        if (current_thread->ticks)
        {
            --current_thread->ticks;

            if (!current_thread->ticks)
            {
                thread_requeue();
            }
        }
    }
}

__attribute__((naked))
ISR(TIMER0_COMPA_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    advance_active_timers();
    advance_thread_ticks();

    if (!thread_select())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}
