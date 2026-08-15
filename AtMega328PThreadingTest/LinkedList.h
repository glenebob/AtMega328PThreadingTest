#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t previous;
    uint8_t next;
} double_linked_list_node_t;

typedef struct
{
    uint8_t first_free;
    uint8_t first_linked;
    uint8_t last_linked;
    double_linked_list_node_t *nodes;
} double_linked_list_t;

void dl_initialize(double_linked_list_t *list, double_linked_list_node_t *first, uint8_t count);
uint8_t dl_create_unlinked(double_linked_list_t *list);
void dl_append(double_linked_list_t *list, uint8_t node_id);
void dl_insert(double_linked_list_t *list, uint8_t node_id, uint8_t before_node_id);
void dl_unlink(double_linked_list_t *list, uint8_t node_id);
void dl_move_first_to_end(double_linked_list_t *list);
void dl_delete_unlinked(double_linked_list_t *list, uint8_t node_id);
uint8_t dl_get_first(double_linked_list_t *list);
