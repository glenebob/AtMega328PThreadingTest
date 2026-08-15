#include <stdint.h>

#include "LinkedList.h"

void dl_initialize(double_linked_list_t *list, double_linked_list_node_t *first, uint8_t count)
{
    list->first_free = 0;
    list->first_linked = 0xFF;
    list->last_linked = 0xFF;
    list->nodes = first;

    uint8_t last_node = count - 1;

    for (uint8_t node_id = 0; node_id < last_node; ++node_id)
    {
        list->nodes[node_id].next = node_id + 1;
    }

    list->nodes[last_node].next = 0xFF;
}

uint8_t dl_create_unlinked(double_linked_list_t *list)
{
    uint8_t node_id = list->first_free;

    if (node_id == 0xFF)
    {
        return node_id;
    }

    list->first_free = list->nodes[node_id].next;
    
    list->nodes[node_id].previous = 0xFF;
    list->nodes[node_id].next = 0xFF;

    return node_id;
}

static void dl_append_internal(double_linked_list_t *list, uint8_t node_id, double_linked_list_node_t *node)
{
    node->next = 0xFF;
    node->previous = list->last_linked;

    if (list->last_linked != 0xFF)
    {
        list->nodes[list->last_linked].next = node_id;
    }

    list->last_linked = node_id;

    if (list->first_linked == 0xFF)
    {
        list->first_linked = node_id;
    }
}

void dl_append(double_linked_list_t *list, uint8_t node_id)
{
    dl_append_internal(list, node_id, &list->nodes[node_id]);
}

void dl_insert(double_linked_list_t *list, uint8_t node_id, uint8_t before_node_id)
{
    double_linked_list_node_t *node = &list->nodes[node_id];

    if (before_node_id == 0xFF)
    {
        dl_append_internal(list, node_id, node);
    }
    else if (before_node_id == list->first_linked)
    {
        node->previous = 0xFF;
        list->nodes[before_node_id].previous = node_id;
        list->first_linked = node_id;
    }
    else
    {
        node->previous = list->nodes[before_node_id].previous;
        list->nodes[list->nodes[before_node_id].previous].next = node_id;
        list->nodes[before_node_id].previous = node_id;
    }

    node->next = before_node_id;
}

void dl_unlink(double_linked_list_t *list, uint8_t node_id)
{
    if (node_id == list->first_linked)
    {
        if (node_id == list->last_linked)
        {
            list->first_linked = 0xFF;
            list->last_linked = 0xFF;
        }
        else
        {
            list->first_linked = list->nodes[node_id].next;
            list->nodes[list->first_linked].previous = 0xFF;
        }
    }
    else if (node_id == list->last_linked)
    {
        list->last_linked = list->nodes[node_id].previous;
        list->nodes[list->last_linked].next = 0xFF;
    }
    else
    {
        list->nodes[list->nodes[node_id].previous].next = list->nodes[node_id].next;
        list->nodes[list->nodes[node_id].next].previous = list->nodes[node_id].previous;
    }
}

void dl_move_first_to_end(double_linked_list_t *list)
{
    uint8_t node_id = list->first_linked;
    double_linked_list_node_t *node = &list->nodes[node_id];

    list->nodes[node->next].previous = 0xFF;
    list->first_linked = node->next;
    node->previous = list->last_linked;
    node->next = 0xFF;
    list->nodes[list->last_linked].next = node_id;
    list->last_linked = node_id;
}

void dl_delete_unlinked(double_linked_list_t *list, uint8_t node_id)
{
    list->nodes[node_id].next = list->first_free;
    list->first_free = node_id;
}

uint8_t dl_get_first(double_linked_list_t *list)
{
    return list->first_linked;
}
