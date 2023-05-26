#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Node.h"

Node node_construtctor(void *data, unsigned long size)
{
    Node nd;
    nd.next = NULL;
    nd.prev = NULL;
    nd.size = size;
    nd.data = malloc(size);
    memcpy(nd.data, data, size);
    return nd;
}

void node_destroy(Node *nd)
{
    free(nd->data);
    free(nd);
}