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
#define STACK_SIZE_IDLE 256
#define STACK_SIZE_USER 256

#define THREAD_USER_TICKS 10

#define PRIORITY_COUNT 4

#define SAVED_REGISTER_BYTE_COUNT 32 + 1

typedef struct
{
    stack_header_t header;
    object_id_t threads[THREAD_USER_COUNT];
} thread_free_list_t;

struct
{
    ring_buffer_header_t header;
    object_id_t thread_id[1];
} kernel_thread_run_queue;

struct
{
    ring_buffer_header_t header;
    object_id_t thread_id[THREAD_USER_COUNT];
} user_thread_run_queue_priority_1;

struct
{
    ring_buffer_header_t header;
    object_id_t thread_id[THREAD_USER_COUNT];
} user_thread_run_queue_priority_2;

static thread_t threads[THREAD_COUNT];
static thread_free_list_t thread_free_list;
static void *run_queues[PRIORITY_COUNT - 1];

uint8_t current_thread_id;
thread_t *current_thread;
uint8_t current_thread_priority;
uint8_t current_thread_ticks;
uint8_t thread_lock_count;

volatile uint32_t idle_spin_count = 0;

static void global_thread_entry(void);
static void thread_idle_entry(void*);

extern uint8_t __data_start;
extern uint8_t _end;

uint16_t malloc_heap_size;

static object_id_t new_thread_id;
static object_id_t new_thread_priority;

static task_queue_t kernel_task_queue;

static void initialize_thread_stack(object_id_t thread_id, thread_t *thread);

int main(void);

void threads_initialize()
{
    // Sanity check to make sure the total stack space does not crash into
    // the data segments.
    uint16_t total_data_bytes = (uint16_t) &_end - (uint16_t ) &__data_start;
    uint16_t total_stacks_size = STACK_SIZE_KERNEL + STACK_SIZE_IDLE + ((THREAD_COUNT - 2) * STACK_SIZE_USER);
    int16_t heap_size = 2048 - total_data_bytes - total_stacks_size;

    if (heap_size < 0)
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
    thread->entry_point = (thread_entry_t) main;
    thread->argument = NULL;

    ++thread_id;
    thread = &threads[thread_id];

    // Idle thread
    initialize_thread_stack(thread_id, thread);

    thread->entry_point = thread_idle_entry;
    thread->argument = NULL;

    stack_initialize(&thread_free_list.header, THREAD_USER_COUNT);

    run_queues[0] = &kernel_thread_run_queue;
    run_queues[1] = &user_thread_run_queue_priority_1;
    run_queues[2] = &user_thread_run_queue_priority_2;

    ring_buffer_initialize(&kernel_thread_run_queue.header, 1);
    ring_buffer_initialize(&user_thread_run_queue_priority_1.header, THREAD_USER_COUNT);
    ring_buffer_initialize(&user_thread_run_queue_priority_2.header, THREAD_USER_COUNT);

    for (object_id_t thread_id = THREAD_COUNT - 1; thread_id >= THREAD_ID_FIRST_USER; --thread_id)
    {
        stack_push(&thread_free_list, &thread_id, sizeof(object_id_t));
    }

    thread_queue(THREAD_ID_KERNEL);
    thread_queue(THREAD_ID_IDLE);

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
        stack_init_block = (stack_init_block_t*) (RAMEND - STACK_SIZE_KERNEL - sizeof(stack_init_block_t) + 1);
    }
    else
    {
        stack_init_block = (stack_init_block_t*) (RAMEND - STACK_SIZE_KERNEL - STACK_SIZE_IDLE - (thread_id - 2) * STACK_SIZE_USER - sizeof(stack_init_block_t) + 1);
    }

    stack_init_block->r1_register = 0;
    stack_init_block->status_register = 0;
    stack_init_block->return_address.high = (uint16_t) global_thread_entry >> 8;
    stack_init_block->return_address.low = (uint16_t) global_thread_entry & 0xFF;

    thread->saved_stack_pointer = (uint16_t) (((void*) stack_init_block) - sizeof(uint16_t*) - (SAVED_REGISTER_BYTE_COUNT - sizeof(stack_init_block_t)) - 1);
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

    thread->status = preemptable ? THREAD_FLAG_PREEMPTABLE : 0;
    thread->entry_point = entry_point;
    thread->argument = argument;

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

    if (thread_lock_count || current_thread_ticks || !thread_select_if())
    {
        sei();

        return;
    }

    thread_yield_dispatch();

    sei();
}

void thread_block_and_select(void)
{
    object_id_t thread_id;

    ring_buffer_pop(run_queues[current_thread_priority], &thread_id, sizeof(object_id_t));
    thread_select();
}

// Must be called with interrupts disabled.
void thread_queue(object_id_t thread_id)
{
    uint8_t priority;

    switch (thread_id)
    {
        case THREAD_ID_KERNEL:
            priority = 0;
            break;

        case THREAD_ID_IDLE:
            return;

       default:
           priority = 1;
           break;
    }

    ring_buffer_push(run_queues[priority], &thread_id, sizeof(object_id_t));
}

void thread_destroy(object_id_t thread_id)
{
    cli();

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
    thread_block_and_select();
    thread_yield_dispatch();

    sei();
}

void thread_select(void)
{
    uint8_t priority_count = PRIORITY_COUNT - 1;

    for (uint8_t i = 0; i < priority_count; ++i)
    {
        if (ring_buffer_peek(run_queues[i], &new_thread_id, sizeof(object_id_t)))
        {
            new_thread_priority = i;

            return;
        }
    }

    new_thread_id = THREAD_ID_IDLE;
    new_thread_priority = priority_count;
}

bool thread_select_if(void)
{
    thread_select();

    return !thread_lock_count && new_thread_id != current_thread_id;
}

void thread_change(void)
{
    current_thread_id = new_thread_id;
    current_thread = &threads[current_thread_id];
    current_thread_priority = new_thread_priority;

    if (current_thread->status & THREAD_FLAG_PREEMPTABLE)
    {
        current_thread_ticks = THREAD_USER_TICKS;
    }
}

__attribute__((naked))
static void global_thread_entry(void)
{
    sei();

    current_thread->entry_point(current_thread->argument);

    cli();

    thread_block_and_select();
    thread_destroy(current_thread_id);
    thread_yield_dispatch();
}

void kernel_init_task(void *root_thread_entry)
{
    timers_initialize();
    thread_create((thread_entry_t) root_thread_entry, NULL, true);
}

__attribute__((OS_task))
void thread_kernel_entry(thread_entry_t root_thread_entry)
{
    static const __flash char string_kernel[] PROGMEM = "Kernel!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_kernel) - 1];

    memcpy_P(status, string_kernel, sizeof(status));

    threads_initialize();
    task_queue_initialize(&kernel_task_queue);

    sei();

    // Queue up remaining initialization.
    task_queue_push(&kernel_task_queue, kernel_init_task, root_thread_entry);

    while (trueFlag)
    {
        task_handler_t handler;
        void *arguments;

        task_queue_pop(&kernel_task_queue, &handler, &arguments);

        static const __flash char string_kernel_task[] PROGMEM = "Kernel Task!";

        char status[sizeof(string_kernel_task) - 1];

        memcpy_P(status, string_kernel_task, sizeof(status));

        handler(arguments);

        memset(status, 0, sizeof(status));
    }

    memset(status, 0, sizeof(status));
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
void thread_yield_dispatch(void)
{
    SAVE_FIRST_HALF_CONTEXT();
    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}

void thread_post_kernel_task(task_handler_t handler, void *arguments)
{
    cli();

    if (thread_lock_count)
    {
        exit(ERR_THREAD_LOCK_COUNT_BAD_LEVEL);
    }

    sei();

    task_queue_push(&kernel_task_queue, handler, arguments);

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
    if (!(current_thread->status & THREAD_FLAG_PREEMPTABLE))
    {
        return;
    }

    if (!current_thread_ticks)
    {
        return;
    }

    if (--current_thread_ticks)
    {
        return;
    }

    switch (current_thread_priority)
    {
        case 1:
        {
            object_id_t thread_id;

            ring_buffer_pop(&user_thread_run_queue_priority_1, &thread_id, sizeof(object_id_t));
            ring_buffer_push(&user_thread_run_queue_priority_2, &thread_id, sizeof(object_id_t));

            current_thread_priority = 2;

            break;
        }

        case 2:
            ring_buffer_rotate(&user_thread_run_queue_priority_2, sizeof(object_id_t));

            break;
    }
}

__attribute__((naked))
ISR(TIMER0_COMPA_vect)
{
    SAVE_FIRST_HALF_CONTEXT_ISR();

    advance_active_timers();
    advance_thread_ticks();

    if (!thread_select_if())
    {
        JUMP(thread_restore_first_half_dispatch);
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_change();

    JUMP(thread_restore_all_dispatch);
}
