#include <stdint.h>
#include <stddef.h>

#include "Types.h"
#include "Interrupt.h"
#include "LinkedList.h"

#include "Thread.h"

#define THREAD_COUNT 10

thread_t threads[THREAD_COUNT];
double_linked_list_node_t thread_list_nodes[THREAD_COUNT];
double_linked_list_t thread_run_queue;

uint8_t runnable_thread_count;
uint8_t current_thread_id;
thread_t *current_thread;

void threads_initialize(thread_entry_t idle_entry, thread_entry_t kernel_entry)
{
    uint8_t last = THREAD_COUNT - 1;

    threads[0].entry_point = idle_entry;
    threads[0].initial_stack_pointer = 0x8FF;
    threads[0].saved_stack_pointer = 0xFFFF;
    threads[0].ticks = 0xFF;

    threads[1].entry_point = kernel_entry;
    threads[1].initial_stack_pointer = 0x8FF - 8;
    threads[1].saved_stack_pointer = 0xFFFF;
    threads[1].ticks = 0xFF;

    threads[2].initial_stack_pointer = 0x8FF - 8 - 0x100;
    threads[2].saved_stack_pointer = 0xFFFF;

    for (uint8_t i = 3; i < last; ++i)
    {
        threads[i].initial_stack_pointer = 0x8FF - 8 - ((i - 1) * 0x100);
        threads[i].saved_stack_pointer = 0xFFFF;
    }

    threads[last].initial_stack_pointer = 0x8FF - 8 - ((last - 1) * 0x100);
    threads[last].saved_stack_pointer = 0xFFFF;

    runnable_thread_count = 0;
    current_thread_id = 0xFF;
    current_thread = NULL;

    dl_initialize(&thread_run_queue, thread_list_nodes, THREAD_COUNT);

    // Create the idle thread (ID: 0) and the kernel task thread (ID: 1),
    // which must remain permanently unlinked from the run queue.
    dl_create_unlinked(&thread_run_queue);
    dl_create_unlinked(&thread_run_queue);
}

uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable)
{
    INTERRUPT_RAISE_LEVEL();

    uint8_t node_id = dl_create_unlinked(&thread_run_queue);

    if (node_id == 0xFF)
    {
        INTERRUPT_LOWER_LEVEL();

        return node_id;
    }

    uint8_t thread_id = node_id;
    thread_t* thread = &threads[thread_id];

    thread->saved_stack_pointer = 0xFFFF;
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

    dl_append(&thread_run_queue, node_id);

    ++runnable_thread_count;

    INTERRUPT_LOWER_LEVEL();

    return thread_id;
}

void thread_remove_from_run_queue(uint8_t thread_id)
{
    INTERRUPT_RAISE_LEVEL();

    dl_unlink(&thread_run_queue, thread_id);

    --runnable_thread_count;

    INTERRUPT_LOWER_LEVEL();
}

// Must be called with interrupts disabled.
void thread_append_to_run_queue(uint8_t thread_id)
{
    dl_append(&thread_run_queue, thread_id);

    ++runnable_thread_count;
}

void thread_move_first_to_end()
{
    dl_move_first_to_end(&thread_run_queue);
}

void thread_delete(uint8_t thread_id)
{
    INTERRUPT_RAISE_LEVEL();

    dl_delete_unlinked(&thread_run_queue, thread_id);

    INTERRUPT_LOWER_LEVEL();
}

uint8_t thread_get_first_runnable()
{
    return dl_get_first(&thread_run_queue);
}

thread_t *thread_get(uint8_t thread_id)
{
    return &threads[thread_id];
}
