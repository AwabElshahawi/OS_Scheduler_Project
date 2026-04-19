#include "priorityqueue.h"

priQueue *createPriQueue(void)
{
    priQueue *q = (priQueue *)malloc(sizeof(priQueue));
    if (q == NULL)
    {
        perror("malloc failed in createPriQueue");
        exit(1);
    }

    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
    return q;
}

void priQueueInsert(priQueue *q, PCB *newProcess)
{
    if (q == NULL || newProcess == NULL)
        return;

    newProcess->next = NULL;
    newProcess->prev = NULL;

    if (q->head == NULL)
    {
        q->head = q->tail = newProcess;
        q->size++;
        return;
    }

    PCB *cur = q->head;

    while (cur != NULL)
    {
        
        if (newProcess->priority < cur->priority)
            break;

        if (newProcess->priority == cur->priority)
        {
            if (newProcess->arrival < cur->arrival)
                break;

            if (newProcess->arrival == cur->arrival &&
                newProcess->id < cur->id)
                break;
        }

        cur = cur->next;
    }

    if (cur == NULL)
    {
        newProcess->prev = q->tail;
        q->tail->next = newProcess;
        q->tail = newProcess;
    }
    else if (cur == q->head)
    {
        newProcess->next = q->head;
        q->head->prev = newProcess;
        q->head = newProcess;
    }
    else
    {
        newProcess->next = cur;
        newProcess->prev = cur->prev;
        cur->prev->next = newProcess;
        cur->prev = newProcess;
    }

    q->size++;
}

PCB *dequeuePriQueue(priQueue *q)
{
    if (q == NULL || q->head == NULL)
        return NULL;

    PCB *temp = q->head;

    /* one element only */
    if (q->head == q->tail)
    {
        q->head = NULL;
        q->tail = NULL;
    }
    else
    {
        q->head = q->head->next;
        q->head->prev = NULL;
    }

    temp->next = NULL;
    temp->prev = NULL;
    q->size--;

    return temp;
}

PCB *peekPriQueue(priQueue *q)
{
    if (q == NULL)
        return NULL;

    return q->head;
}

bool isPriorityQueueEmpty(priQueue *q)
{
    return (q == NULL || q->head == NULL);
}

void printPriQueue(priQueue *q)
{
    if (q == NULL)
        return;

    PCB *cur = q->head;
    while (cur != NULL)
    {
        printf("[id=%d pr=%d arr=%d] ", cur->id, cur->priority, cur->arrival);
        cur = cur->next;
    }
    printf("\n");
}

void destroyPriQueue(priQueue *q)
{
    if (q == NULL)
        return;

    free(q);
}