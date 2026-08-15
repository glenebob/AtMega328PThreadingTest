#pragma once

typedef void (*thread_entry_t)(void*);

typedef struct thread_t
{
    thread_entry_t entry_point;
    void *argument;
    uint16_t initial_stack_pointer;
    uint16_t saved_stack_pointer;
    uint8_t ticks;
} thread_t;

extern uint8_t runnable_thread_count;
extern uint8_t current_thread_id;
extern thread_t *current_thread;

void threads_initialize();
uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable);
void thread_remove_from_run_queue(uint8_t thread_id);
void thread_append_to_run_queue(uint8_t thread_id);
void thread_move_first_to_end();
void thread_delete(uint8_t thread_id);
uint8_t thread_get_first_runnable();
thread_t *thread_get(uint8_t thread_id);
