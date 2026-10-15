#pragma once

#include "Types.h"

typedef void (*task_handler_t)(void*);

void task_queue_initialize(void);
bool task_queue_push(task_handler_t handler, void *arguments);
bool task_queue_pop(task_handler_t *handler, void **arguments);
