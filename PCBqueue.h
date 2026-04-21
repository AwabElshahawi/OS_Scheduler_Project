#ifndef PCB_QUEUE_H
#define PCB_QUEUE_H

#include "headers.h"

typedef struct PCBNode
{
    PCB *data;
    struct PCBNode *next;
} PCBNode;

typedef struct
{
    PCBNode *rear;
} PCBCircularQueue;

void initPCBQueue(PCBCircularQueue *q)
{
    q->rear = NULL;
}

bool isPCBQueueEmpty(PCBCircularQueue *q)
{
    return q->rear == NULL;
}

void enqueuePCB(PCBCircularQueue *q, PCB *data)
{
    PCBNode *newNode = (PCBNode *)malloc(sizeof(PCBNode));
    if (!newNode)
        return;

    newNode->data = data;

    if (isPCBQueueEmpty(q))
    {
        newNode->next = newNode;
        q->rear = newNode;
    }
    else
    {
        newNode->next = q->rear->next;
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

bool dequeuePCB(PCBCircularQueue *q, PCB **retData)
{
    if (isPCBQueueEmpty(q))
    {
        *retData = NULL;
        return false;
    }

    PCBNode *temp = q->rear->next;
    *retData = temp->data;

    if (q->rear == temp)
    {
        q->rear = NULL;
    }
    else
    {
        q->rear->next = temp->next;
    }

    free(temp);
    return true;
}

PCB *peekPCBQueue(PCBCircularQueue *q)
{
    if (isPCBQueueEmpty(q))
        return NULL;

    return q->rear->next->data;
}

#endif