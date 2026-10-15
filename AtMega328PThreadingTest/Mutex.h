#pragma once

#include "RingBuffer.h"
#include "Thread.h"

#define MUTEX_WAIT_QUEUE_COUNT THREAD_USER_COUNT - 1

typedef struct
{
    ring_buffer_header_t header;
    object_id_t tasks[MUTEX_WAIT_QUEUE_COUNT];
} mutex_wait_queue_t;

typedef struct
{
    object_id_t thread_id;
    uint8_t use_count;
    mutex_wait_queue_t waiters;
} mutex_t;

void *mutex_create(void);
void mutex_initialize(void *mutex_data);
void mutex_destroy(void *mutex_data);
bool mutex_lock(void *mutex_data);
bool mutex_unlock(void *mutex_data);
