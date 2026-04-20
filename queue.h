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

ProcessData *dequeue(CircularQueue *q)
{
    if (isEmpty(q))
        return NULL;

    Node *temp = q->rear->next;
    ProcessData *data = temp->data;

    if (q->rear == temp)
        q->rear = NULL;
    else
        q->rear->next = temp->next;

    free(temp);
    return data;
}

ProcessData *peekQueue(CircularQueue *q)
{
    if (isEmpty(q))
        return NULL;

    return q->rear->next->data;
}

#endif