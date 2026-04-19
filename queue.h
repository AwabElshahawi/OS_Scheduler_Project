#ifndef QUEUE_H
#define QUEUE_H

#include "headers.h"

typedef struct Node
{
    PCB *pcb;
    struct Node *next;
} Node;

typedef struct
{
    Node *rear;
} CircularQueue;

void initQueue(CircularQueue *q);
bool isEmpty(CircularQueue *q);
void enqueue(CircularQueue *q, PCB *pcb);
bool dequeue(CircularQueue *q, PCB **retpcb);

#endif