#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

typedef struct PCB
{
    int id;
    int arrival_time;
    int runtime;
    int priority;

    int state;

    int remaining_time;
    int waiting_time;

    int pid;

    int start_time;
    int finish_time;
    int last_start_time;

    int executed_time;

    int TA;
    float WTA;

    struct PCB *next;
    struct PCB *prev;
} PCB;

typedef struct Node
{
    PCB *pcb;
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
///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================



int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
