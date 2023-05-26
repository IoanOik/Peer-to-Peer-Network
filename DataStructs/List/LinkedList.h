#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "../Node/Node.h"

typedef struct LinkedList
{
    Node *head;
    int length;
} LinkedList;

LinkedList ll_constructor();
void ll_remove(LinkedList *list, int index);
void ll_destroy(LinkedList *list);
void ll_push(LinkedList *list, void *data, unsigned long size);
void *ll_retrieve(LinkedList *list, int index);
unsigned long ll_retrieve_size(LinkedList *list, int index);
void ll_sort(LinkedList *list, int (*compare)(const void *a,const void *b));

#endif // LINKEDLIST_H