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

void initQueue(CircularQueue *q)
{
    q->rear = NULL;
}

bool isEmpty(CircularQueue *q)
{
    return q->rear == NULL;
}

void enqueue(CircularQueue *q, ProcessData *data)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode)
        return;

    newNode->data = data;

    if (isEmpty(q))
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

bool dequeue(CircularQueue *q, ProcessData **retData)
{
    if (isEmpty(q))
    {
        *retData = NULL;
        return false;
    }

    Node *temp = q->rear->next;
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
ProcessData *peekQueue(CircularQueue *q)
{
    if (isEmpty(q))
        return NULL;

    return q->rear->next->data;
}

#endif