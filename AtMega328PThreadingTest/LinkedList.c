#include <stdint.h>

#include "Types.h"

#include "LinkedList.h"

void dl_initialize(double_linked_list_t *list, double_linked_list_node_t *first_node, uint8_t count)
{
    list->first_linked = OBJECT_ID_INVALID;
    list->last_linked = OBJECT_ID_INVALID;
    list->nodes = first_node;

    uint8_t last_node = count - 1;

    for (uint8_t node_id = 0; node_id < last_node; ++node_id)
    {
        list->nodes[node_id].next = node_id + 1;
    }

    list->nodes[last_node].next = OBJECT_ID_INVALID;
}

void dl_append(double_linked_list_t *list, uint8_t node_id)
{
    double_linked_list_node_t *node = &list->nodes[node_id];

    node->next = OBJECT_ID_INVALID;
    node->previous = list->last_linked;

    if (list->last_linked != OBJECT_ID_INVALID)
    {
        list->nodes[list->last_linked].next = node_id;
    }

    list->last_linked = node_id;

    if (list->first_linked == OBJECT_ID_INVALID)
    {
        list->first_linked = node_id;
    }
}

void dl_insert(double_linked_list_t *list, uint8_t node_id, uint8_t before_node_id)
{
    double_linked_list_node_t *nodes = list->nodes;
    double_linked_list_node_t *node = &nodes[node_id];

    if (before_node_id == OBJECT_ID_INVALID)
    {
        node->previous = list->last_linked;

        if (list->last_linked != OBJECT_ID_INVALID)
        {
            list->nodes[list->last_linked].next = node_id;
        }

        list->last_linked = node_id;

        if (list->first_linked == OBJECT_ID_INVALID)
        {
            list->first_linked = node_id;
        }
    }
    else if (before_node_id == list->first_linked)
    {
        node->previous = OBJECT_ID_INVALID;
        nodes[before_node_id].previous = node_id;
        list->first_linked = node_id;
    }
    else
    {
        node->previous = nodes[before_node_id].previous;
        nodes[nodes[before_node_id].previous].next = node_id;
        nodes[before_node_id].previous = node_id;
    }

    node->next = before_node_id;
}

void dl_remove(double_linked_list_t *list, uint8_t node_id)
{
    double_linked_list_node_t *nodes = list->nodes;

    if (node_id == list->first_linked)
    {
        if (node_id == list->last_linked)
        {
            list->first_linked = OBJECT_ID_INVALID;
            list->last_linked = OBJECT_ID_INVALID;
        }
        else
        {
            list->first_linked = nodes[node_id].next;
            nodes[list->first_linked].previous = OBJECT_ID_INVALID;
        }
    }
    else if (node_id == list->last_linked)
    {
        list->last_linked = nodes[node_id].previous;
        nodes[list->last_linked].next = OBJECT_ID_INVALID;
    }
    else
    {
        nodes[nodes[node_id].previous].next = nodes[node_id].next;
        nodes[nodes[node_id].next].previous = nodes[node_id].previous;
    }
}

uint8_t dl_get_first(double_linked_list_t *list)
{
    return list->first_linked;
}

uint8_t dl_get_last(double_linked_list_t *list)
{
    return list->last_linked;
}
