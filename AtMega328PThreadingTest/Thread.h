#pragma once

#include "TestApplication.h"
#include "Types.h"
#include "Task.h"

typedef void (*thread_entry_t)(void*);

typedef struct thread_t
{
    thread_entry_t entry_point;
    void *argument;
    uint16_t saved_stack_pointer;
    uint8_t status;
} thread_t;

#define THREAD_FLAG_PREEMPTABLE 1

#define THREAD_ID_KERNEL 0
#define THREAD_ID_IDLE 1
#define THREAD_ID_FIRST_USER 2

#define THREAD_COUNT THREAD_USER_COUNT + 2

extern uint16_t malloc_heap_size;

extern uint8_t current_thread_id;
extern thread_t *current_thread;
extern uint8_t current_thread_priority;
extern uint8_t thread_lock_count;

extern volatile uint32_t idle_spin_count;

void threads_initialize(void);
uint8_t thread_create_unlinked(thread_entry_t entry_point, void *argument, bool preemptable);
uint8_t thread_create(thread_entry_t entry_point, void *argument, bool preemptable);
void thread_lock(void);
void thread_unlock(void);
void thread_block_and_select(void);
void thread_kernel_dormant(void);
void thread_queue(object_id_t thread_id);
void thread_requeue(void);
void thread_destroy(object_id_t thread_id);
void thread_sleep(uint16_t milliseconds);
void thread_select(void);
bool thread_select_if(void);
void thread_change(void);
void thread_yield_dispatch(void);
void thread_yield_dispatch_NEW(void);
void thread_post_kernel_task(task_handler_t handler, void *arguments);
void thread_restore_first_half_dispatch(void);
void thread_restore_all_dispatch(void);
void thread_kernel_entry(thread_entry_t root_thread_entry);
