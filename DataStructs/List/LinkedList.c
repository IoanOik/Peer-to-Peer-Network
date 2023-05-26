#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LinkedList.h"

LinkedList ll_constructor()
{
    LinkedList new_list;
    new_list.head = NULL;
    new_list.length = 0;
    return new_list;
}

void ll_remove(LinkedList *list, int index)
{
    Node *node_to_remove;
    if (index == 0)
    {
        /* The first node shall be removed*/
        node_to_remove = list->head;
        if (node_to_remove)
        {
            list->head = node_to_remove->next;
            node_destroy(node_to_remove);
        }
    }
    else
    {
        Node *temp = list->head;
        for (int i = 0; i < index - 1; i++)
        {
            temp = temp->next;
        }
        node_to_remove = temp->next;
        temp->next = node_to_remove->next;
        temp->next->prev = node_to_remove->prev;
        node_destroy(node_to_remove);
    }
    list->length -= 1;
}

void ll_destroy(LinkedList *list)
{
    for (int i = 0; i < list->length; i++)
    {
        ll_remove(list, 0);
    }
}

void ll_push(LinkedList *list, void *data, unsigned long size)
{
    Node *node_to_push = (Node *)malloc(sizeof(Node));
    *node_to_push = node_construtctor(data, size);
    if (list->length == 0)
    {
        node_to_push->next = list->head;
        list->head = node_to_push;
    }
    else
    {
        Node *temp = list->head;
        for (int i = 0; i < list->length - 1; i++)
        {
            temp = temp->next;
        }
        temp->next = node_to_push;
        node_to_push->prev = temp;
    }
    list->length += 1;
}

void *ll_retrieve(LinkedList *list, int index)
{
    Node *temp = list->head;
    for (int i = 0; i < index; i++)
    {
        temp = temp->next;
    }
    return temp->data;
}

unsigned long ll_retrieve_size(LinkedList *list, int index)
{
    Node *temp = list->head;
    for (int i = 0; i < index; i++)
    {
        temp = temp->next;
    }
    return temp->size;
}

void ll_sort(LinkedList *list, int (*compare)(const void *a, const void *b))
{
    for (Node *i = list->head; i; i = i->next)
    {
        for (Node *j = i->next; j; j = j->next)
        {
            if (compare(i->data, j->data) > 0)
            {
                // Swap them.
                void *temporary = j->data;
                j->data = i->data;
                i->data = temporary;
            }
        }
    }
}