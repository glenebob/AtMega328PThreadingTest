#pragma once

#include "TestApplication.h"
#include "Types.h"
#include "Stack.h"
#include "RingBuffer.h"

typedef void (*task_handler_t)(void*);

typedef struct
{
    task_handler_t handler;
    void *arguments;
} task_t;

typedef struct
{
    task_t tasks[TASK_COUNT];
    struct
    {
        stack_header_t header;
        object_id_t tasks[TASK_COUNT];
    } task_free_list;
    struct
    {
        ring_buffer_header_t header;
        object_id_t tasks[TASK_COUNT];
    } task_queue;
    object_id_t waiting_thread_id;
} task_queue_t;

void *task_queue_create(void);
void task_queue_initialize(void *task_queue_data);
void task_queue_destroy(void *task_queue_data);
bool task_queue_push(void *task_queue_data, task_handler_t handler, void *arguments);
void task_queue_pop(void *task_queue_data, task_handler_t *handler, void **arguments);
