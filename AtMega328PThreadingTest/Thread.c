#include <stdint.h>
#include <stddef.h>

#include "Types.h"
#include "Interrupt.h"
#include "LinkedList.h"
#include "Timer.h"
#include "Context.h"

#include "Thread.h"

#define THREAD_COUNT 6

#define STACK_SIZE_USER 256
#define STACK_SIZE_KERNEL 256
#define STACK_SIZE_IDLE 64

static thread_t threads[THREAD_COUNT];
static double_linked_list_node_t thread_nodes[THREAD_COUNT];
static double_linked_list_t thread_run_queue;

uint8_t runnable_thread_count;
uint8_t current_thread_id;
thread_t *current_thread;
bool kernel_thread_is_active;

volatile uint32_t idle_spin_count = 0;

static void global_thread_entry();

extern uint8_t __data_start;
extern uint8_t _end;

void threads_initialize(thread_entry_t idle_entry)
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

    // Sandbox malloc to the region below the total stack space.
    __malloc_heap_end = (char*) (0x08FF - total_stacks_size);

    uint8_t thread_id = 0;

    // Kernel thread - currently running
    threads[thread_id].status = THREAD_STATUS_RUNNABLE;
    threads[thread_id].ticks = 0xFF;

    ++thread_id;

    // Idle thread
    threads[thread_id].status = THREAD_STATUS_FREE;

    // User threads
    for (++thread_id; thread_id < THREAD_COUNT; ++thread_id)
    {
        threads[thread_id].status = THREAD_STATUS_FREE;
    }

    runnable_thread_count = 0;
    current_thread_id = OBJECT_ID_INVALID;
    current_thread = NULL;

    dl_initialize(&thread_run_queue, &thread_nodes[0], THREAD_COUNT);

    // Create the kernel thread (ID: 0). We are running in the kernel thread,
    // so we don't need to set the saved stack pointer or initialize
    // its stack.
    // This thread is fully initialized already.
    dl_create_unlinked(&thread_run_queue);

    kernel_thread_is_active = true;
}

uint8_t thread_create_unlinked(thread_entry_t entry_point, void *argument, bool preemptable)
{
    INTERRUPT_RAISE_LEVEL();

    uint8_t node_id = dl_create_unlinked(&thread_run_queue);

    INTERRUPT_LOWER_LEVEL();

    if (node_id == OBJECT_ID_INVALID)
    {
        return node_id;
    }

    uint8_t thread_id = node_id;
    thread_t* thread = &threads[thread_id];

    typedef struct
    {
        uint8_t r1_register;
        uint8_t r0_register_placeholder_not_used;
        uint8_t status_register;
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
        stack_init_block = (stack_init_block_t*) (0x8FF - STACK_SIZE_KERNEL - STACK_SIZE_IDLE - ((thread_id - 2) * STACK_SIZE_USER) - sizeof(stack_init_block_t) + 1);
    }

    stack_init_block->r1_register = 0;
    stack_init_block->status_register = 0;
    stack_init_block->return_address.high = (uint16_t) global_thread_entry >> 8;
    stack_init_block->return_address.low = (uint16_t) global_thread_entry & 0xFF;

    thread->saved_stack_pointer = (uint16_t) (((void*) stack_init_block) - sizeof(uint16_t*) - 29);
    thread->entry_point = entry_point;
    thread->argument = argument;

    if (preemptable)
    {
        thread->ticks = 0;
    }
    else
    {
        thread->ticks = 0xFF;
    }

    thread->status = THREAD_STATUS_RUNNABLE;

    return thread_id;
}

uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable)
{
    uint8_t thread_id = thread_create_unlinked(entry_point, argument, preemptable);

    if (thread_id == OBJECT_ID_INVALID)
    {
        return thread_id;
    }

    INTERRUPT_RAISE_LEVEL();

    dl_append(&thread_run_queue, thread_id);

    ++runnable_thread_count;

    INTERRUPT_LOWER_LEVEL();

    return thread_id;
}

void thread_block(uint8_t thread_id)
{
    INTERRUPT_RAISE_LEVEL();

    threads[thread_id].status = THREAD_STATUS_BLOCKED;

    if (thread_id != THREAD_ID_KERNEL)
    {
        dl_unlink(&thread_run_queue, thread_id);
        --runnable_thread_count;
    }

    INTERRUPT_LOWER_LEVEL();
}

// Must be called with interrupts disabled.
void thread_ensure_runnable(uint8_t thread_id)
{
    if (threads[thread_id].status == THREAD_STATUS_BLOCKED)
    {
        threads[thread_id].status = THREAD_STATUS_RUNNABLE;

        if (thread_id != THREAD_ID_KERNEL)
        {
            dl_append(&thread_run_queue, thread_id);
            ++runnable_thread_count;
        }
    }
}

void thread_move_first_to_end()
{
    dl_move_first_to_end(&thread_run_queue);
}

void thread_delete(uint8_t thread_id)
{
    INTERRUPT_RAISE_LEVEL();

    threads[thread_id].status = THREAD_STATUS_FREE;

    dl_delete_unlinked(&thread_run_queue, thread_id);

    INTERRUPT_LOWER_LEVEL();
}

void thread_sleep(uint16_t milliseconds)
{
	if (!milliseconds)
	{
		return;
	}

	INTERRUPT_RAISE_LEVEL();

	timer_add(current_thread_id, milliseconds);
	thread_block(current_thread_id);
	thread_yield_dispatch();
}

uint8_t thread_get_first_runnable()
{
    object_id_t thread_id = THREAD_ID_IDLE;

    if (kernel_thread_is_active && threads[THREAD_ID_KERNEL].status == THREAD_STATUS_RUNNABLE)
    {
        return THREAD_ID_KERNEL;
    }

    if (current_thread_id != THREAD_ID_KERNEL)
    {
        thread_id = dl_get_first(&thread_run_queue);

        return thread_id != OBJECT_ID_INVALID ? thread_id : THREAD_ID_IDLE;
    }
    else
    {
        return thread_id;
    }
}

void thread_set_current(uint8_t thread_id)
{
    current_thread_id = thread_id;
    current_thread = &threads[thread_id];
}

__attribute__((naked))
static void global_thread_entry()
{
    current_thread->entry_point(current_thread->argument);

    INTERRUPT_RAISE_LEVEL();

    thread_block(current_thread_id);
    thread_delete(current_thread_id);

    thread_set_current(thread_get_first_runnable());

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();
    RESTORE_FIRST_HALF_CONTEXT();
    INTERRUPT_LOWER_LEVEL_NO_ENABLE();

    reti();
}

__attribute__((naked))
void thread_yield_dispatch()
{
    SAVE_FIRST_HALF_CONTEXT();
    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_set_current(thread_get_first_runnable());

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();
    RESTORE_FIRST_HALF_CONTEXT();
    INTERRUPT_LOWER_LEVEL_NO_ENABLE();

    reti();
}

__attribute__((naked))
void thread_dispatch_to_kernel()
{
    SAVE_FIRST_HALF_CONTEXT();
    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    thread_ensure_runnable(THREAD_ID_KERNEL);
    thread_set_current(THREAD_ID_KERNEL);

    kernel_thread_is_active = true;

    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();
    RESTORE_FIRST_HALF_CONTEXT();
    INTERRUPT_LOWER_LEVEL_NO_ENABLE();

    reti();
}

static bool need_to_switch_threads()
{
    if (current_thread_id == THREAD_ID_KERNEL)
    {
        return false;
    }
    else if (current_thread_id == THREAD_ID_IDLE)
    {
        if (kernel_thread_is_active)
        {
            return threads[THREAD_ID_KERNEL].status == THREAD_STATUS_RUNNABLE;
        }
        else
        {
            return runnable_thread_count;
        }
    }
    else if (runnable_thread_count < 2)
    {
        return false;
    }
    else if (current_thread->ticks != 0xFF)
    {
        if (current_thread->ticks)
        {
            --current_thread->ticks;
        }

        return !current_thread->ticks && runnable_thread_count > 1;
    }

    return false;
}

__attribute__((naked))
ISR(TIMER0_COMPA_vect)
{
    SAVE_FIRST_HALF_CONTEXT();

    advance_active_timers();

    if (!need_to_switch_threads())
    {
        goto return_no_context_switch;
    }

    SAVE_SECOND_HALF_CONTEXT();
    SAVE_STACK_POINTER();

    if (current_thread_id != THREAD_ID_IDLE)
    {
        thread_move_first_to_end();
    }

    thread_set_current(thread_get_first_runnable());

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    RESTORE_STACK_POINTER();
    RESTORE_SECOND_HALF_CONTEXT();
    RESTORE_FIRST_HALF_CONTEXT();

    reti();

return_no_context_switch:
    RESTORE_FIRST_HALF_CONTEXT();

    reti();
}
