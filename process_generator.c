
#include "headers.h"
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
int msgq_id = -1;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    if (argc < 2)
    {
        printf("Usage: %s <input file>\n", argv[0]);
        exit(1);
    }

    CircularQueue *processes = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(processes);

    FILE *f = fopen("test1hpf.txt", "r");
    if (f == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }

    int id, arrival, runtime, priority;
    fscanf(f, "%*[^\n]\n");

    while (fscanf(f, "%d %d %d %d", &id, &arrival, &runtime, &priority) == 4)
    {
        ProcessData *p = (ProcessData *)malloc(sizeof(ProcessData));
        p->id = id;
        p->arrival_time = arrival;
        p->runtime = runtime;
        p->priority = priority;
        enqueue(processes, p);
    }
    fclose(f);

    printf("\nEnter a scheduling algorithm:\n");
    printf("1 - Preemptive Highest Priority First (HPF)\n");
    printf("2 - Round Robin (RR)\n");
    printf("3 - First Come First Served (FCFS)\n");
    printf("Please enter the number corresponding to your choice: ");

    int chosensched;
    scanf("%d", &chosensched);

    int quantum = 0;
    if (chosensched == 3)
    {
        printf("Enter quantum: ");
        scanf("%d", &quantum);
        while (quantum <= 0)
        {
            printf("Quantum must be greater than zero!\n");
            scanf("%d", &quantum);
        }
    }

    msgq_id = msgget(MSGKEY, IPC_CREAT | 0666);
    if (msgq_id == -1)
    {
        perror("msgget failed");
        exit(1);
    }

    pid_t clk_pid = fork();
    if (clk_pid == 0)
    {
        execl("./clk.out", "clk.out", NULL);
        perror("execl clk failed");
        exit(1);
    }

    pid_t scheduler_pid = fork();
    if (scheduler_pid == 0)
    {
        char algoStr[10], quantumStr[10];
        sprintf(algoStr, "%d", chosensched);
        sprintf(quantumStr, "%d", quantum);

        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);
        perror("execl scheduler failed");
        exit(1);
    }

    initClk();

    int lastClk = -1;

    while (!isEmpty(processes))
    {
        int now = getClk();
        if (now == lastClk)
            continue;
        lastClk = now;

        while (!isEmpty(processes))
        {
            ProcessData *p = peekQueue(processes);

            if (p->arrival_time <= now)
            {
                ProcessData *sent = NULL;
                dequeue(processes, &sent);

                ProcessMessage msg;
                msg.mtype = 1;
                msg.isLast = 0;
                msg.p = *sent;

                if (msgsnd(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), !IPC_NOWAIT) == -1)
                    perror("msgsnd failed");

                free(sent);
            }
            else
            {
                break;
            }
        }
    }

    ProcessMessage endMsg;
    endMsg.mtype = 1;
    endMsg.isLast = 1;

    if (msgsnd(msgq_id, &endMsg, sizeof(ProcessMessage) - sizeof(long), !IPC_NOWAIT) == -1)
        perror("msgsnd end failed");

    waitpid(scheduler_pid, NULL, 0);

    destroyClk(true);
    return 0;
}

void clearResources(int signum)
{
    if (msgq_id != -1)
        msgctl(msgq_id, IPC_RMID, NULL);
}