#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t previous;
    uint8_t next;
} double_linked_list_node_t;

typedef struct
{
    uint8_t first_linked;
    uint8_t last_linked;
    double_linked_list_node_t *nodes;
} double_linked_list_t;

void dl_initialize(double_linked_list_t *list, double_linked_list_node_t *first_node, uint8_t count);
void dl_append(double_linked_list_t *list, uint8_t node_id);
void dl_insert(double_linked_list_t *list, uint8_t node_id, uint8_t before_node_id);
void dl_remove(double_linked_list_t *list, uint8_t node_id);
uint8_t dl_get_first(double_linked_list_t *list);
uint8_t dl_get_last(double_linked_list_t *list);
