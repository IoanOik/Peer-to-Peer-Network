#ifndef NODE_H
#define NODE_H

typedef struct Node
{
    void *data;
    unsigned long size;
    struct Node *next;
    struct Node *prev;
} Node;

Node node_construtctor(void *data, unsigned long size);
void node_destroy(Node *nd);

#endif // NODE_H