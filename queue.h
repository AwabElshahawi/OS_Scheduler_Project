#ifndef QUEUE_H
#define QUEUE_H

#include "headers.h"

typedef struct Node
{
    ProcessData *data;
    struct Node *next;
} Node;

typedef struct
{
    Node *rear;
} CircularQueue;

void initQueue(CircularQueue *q);
bool isEmpty(CircularQueue *q);
void enqueue(CircularQueue *q, ProcessData *data);
bool dequeue(CircularQueue *q, ProcessData **retData);
ProcessData *peekQueue(CircularQueue *q);

#endif