#pragma once

typedef void (*task_handler_t)(void*);

void task_queue_init();
void task_queue_push(task_handler_t handler, void *arguments);
bool task_queue_pop(task_handler_t *handler, void **arguments);
