#pragma once

#include "Types.h"

typedef void (*thread_entry_t)(void*);

typedef struct thread_t
{
    thread_entry_t entry_point;
    void *argument;
    uint16_t saved_stack_pointer;
    uint8_t ticks;
    uint8_t status;
} thread_t;

#define THREAD_STATUS_DORMANT 0
#define THREAD_STATUS_RUNNABLE 1
#define THREAD_STATUS_BLOCKED 2

#define THREAD_ID_KERNEL 0
#define THREAD_ID_IDLE 1
#define THREAD_ID_FIRST_USER 2

#define THREAD_COUNT 6
#define THREAD_USER_COUNT THREAD_COUNT - THREAD_ID_FIRST_USER

extern uint16_t malloc_heap_size;

extern uint8_t runnable_thread_count;
extern uint8_t current_thread_id;
extern thread_t *current_thread;
extern bool kernel_thread_is_active;

extern volatile uint32_t idle_spin_count;

void threads_initialize(void);
uint8_t thread_create_unlinked(thread_entry_t entry_point, void *argument, bool preemptable);
uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable);
void thread_block(void);
void thread_kernel_dormant(void);
void thread_queue(object_id_t thread_id);
void thread_requeue(void);
void thread_destroy(object_id_t thread_id);
void thread_sleep(uint16_t milliseconds);
bool thread_select(object_id_t *new_thread_id);
void thread_select_and_change(void);
void thread_change(object_id_t thread_id);
void thread_yield_dispatch(void);
void thread_restore_first_half_dispatch(void);
void thread_restore_all_dispatch(void);
