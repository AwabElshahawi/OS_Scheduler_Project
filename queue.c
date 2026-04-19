#include "queue.h"

void initQueue(CircularQueue *q)
{
    q->rear = NULL;
}

bool isEmpty(CircularQueue *q)
{
    return q->rear == NULL;
}

void enqueue(CircularQueue *q, PCB *pcb)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode)
    {
        printf("Memory allocation failed\n");
        return;
    }
    if (pcb != NULL)
    {
        newNode->pcb = pcb;

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
    else
    {
        printf("PCB is NULL \n");
        free(newNode);
        return;
    }
}

bool dequeue(CircularQueue *q, PCB **retpcb)
{
    if (isEmpty(q))
    {
        *retpcb = NULL;
        return false; 
    }

    Node *temp = q->rear->next;
    *retpcb = temp->pcb;

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