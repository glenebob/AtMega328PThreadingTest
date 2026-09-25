#pragma once

typedef void (*thread_entry_t)(void*);

typedef struct thread_t
{
    thread_entry_t entry_point;
    void *argument;
    uint16_t saved_stack_pointer;
    uint8_t ticks;
    uint8_t status;
} thread_t;

#define THREAD_STATUS_FREE 0
#define THREAD_STATUS_RUNNABLE 1
#define THREAD_STATUS_BLOCKED 2

#define THREAD_ID_KERNEL 0
#define THREAD_ID_IDLE 1
#define THREAD_ID_FIRST_USER 2

extern uint8_t runnable_thread_count;
extern uint8_t current_thread_id;
extern thread_t *current_thread;
extern bool kernel_thread_is_active;

extern volatile uint32_t idle_spin_count;

void threads_initialize(thread_entry_t idle_entry);
uint8_t thread_create_unlinked(thread_entry_t entry_point, void *argument, bool preemptable);
uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable);
void thread_block(uint8_t thread_id);
void thread_ensure_runnable(uint8_t thread_id);
void thread_move_first_to_end();
void thread_delete(uint8_t thread_id);
void thread_sleep(uint16_t milliseconds);
uint8_t thread_get_first_runnable();
//thread_t *thread_get(uint8_t thread_id);
void thread_set_current(uint8_t thread_id);
void thread_yield_dispatch();
void thread_dispatch_to_kernel();
