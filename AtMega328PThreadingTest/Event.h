#pragma once

#include "RingBuffer.h"
#include "Thread.h"

#define EVENT_WAIT_QUEUE_COUNT THREAD_USER_COUNT

#define EVENT_TRIGGERED 1
#define EVENT_AUTO_RESET 2

typedef struct
{
    ring_buffer_header_t header;
    object_id_t threads[EVENT_WAIT_QUEUE_COUNT];
} event_wait_queue_t;

typedef struct
{
    uint8_t flags;
    event_wait_queue_t waiters;
} event_t;

void *event_create(uint8_t flags);
void event_initialize(void *event_data, uint8_t flags);
void event_destroy(void *event_data);
bool event_wait(void *event_data);
void event_trigger(void *event_data);
void event_reset(void *event_data);
