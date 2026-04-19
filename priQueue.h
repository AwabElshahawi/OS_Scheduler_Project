#ifndef PRIORITYQUEUE_H
#define PRIORITYQUEUE_H

#include "headers.h"

typedef struct priQueue
{
    int size;
    PCB *head;
    PCB *tail;
} priQueue;

priQueue *createPriQueue(void);
void priQueueInsert(priQueue *q, PCB *newProcess);
PCB *dequeuePriQueue(priQueue *q);
PCB *peekPriQueue(priQueue *q);


bool isPriorityQueueEmpty(priQueue *q);
void printPriQueue(priQueue *q);
void destroyPriQueue(priQueue *q);

#endif