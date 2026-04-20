#include "headers.h"
#include "queue.h"

int msgq_id = -1;

void clearResources(int signum)
{
    if (msgq_id != -1)
        msgctl(msgq_id, IPC_RMID, NULL);
}

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
/* give clock a moment to create shared memory */
sleep(1);
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

    while (!isEmpty(processes))
    {
        ProcessData *p = peekQueue(processes);
        int now = getClk();
        usleep(100000); // Sleep for 100ms to reduce CPU usage
       
        if (p->arrival_time == now)
        {
            ProcessData *sent = NULL;
            dequeue(processes, &sent);

            ProcessMessage msg;
            msg.mtype  = 1;
            msg.isLast = 0;
            msg.p.id           = p->id;
            msg.p.arrival_time = p->arrival_time;
            msg.p.runtime      = p->runtime;
            msg.p.priority     = p->priority;
            if (msgsnd(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0) == -1)
                perror("msgsnd failed");

        }
    }

    ProcessMessage endMsg;
    endMsg.mtype = 1;
    endMsg.isLast = 1;

    if (msgsnd(msgq_id, &endMsg, sizeof(ProcessMessage) - sizeof(long), 0) == -1)
        perror("msgsnd end failed");

    waitpid(scheduler_pid, NULL, 0);

    destroyClk(true);
    return 0;
}







// #include "headers.h"
// #include "queue.h"

// int msgq_id = -1;

// void clearResources(int signum)
// {
//     if (msgq_id != -1)
//         msgctl(msgq_id, IPC_RMID, NULL);
// }

// int main(int argc, char *argv[])
// {
//     signal(SIGINT, clearResources);

//     if (argc < 2)
//     {
//         printf("Usage: %s <input file>\n", argv[0]);
//         exit(1);
//     }

//     CircularQueue *processes = (CircularQueue *)malloc(sizeof(CircularQueue));
//     if (processes == NULL)
//     {
//         perror("malloc failed");
//         exit(1);
//     }
//     initQueue(processes);

//     FILE *f = fopen(argv[1], "r");
//     if (f == NULL)
//     {
//         perror("Error opening file");
//         exit(1);
//     }

//     int id, arrival, runtime, priority;

//     /* skip header/comment line */
//     fscanf(f, "%*[^\n]\n");

//     while (fscanf(f, "%d %d %d %d", &id, &arrival, &runtime, &priority) == 4)
//     {
//         ProcessData *p = (ProcessData *)malloc(sizeof(ProcessData));
//         if (p == NULL)
//         {
//             perror("malloc failed for ProcessData");
//             fclose(f);
//             exit(1);
//         }

//         p->id = id;
//         p->arrival_time = arrival;
//         p->runtime = runtime;
//         p->priority = priority;

//         enqueue(processes, p);
//     }
//     fclose(f);

//     printf("\nEnter a scheduling algorithm:\n");
//     printf("1 - Preemptive Highest Priority First (HPF)\n");
//     printf("2 - Round Robin (RR)\n");
//     printf("3 - First Come First Served (FCFS)\n");
//     printf("Please enter the number corresponding to your choice: ");

//     int chosensched;
//     scanf("%d", &chosensched);

//     int quantum = 0;

//     /* RR is choice 2, not 3 */
//     if (chosensched == 2)
//     {
//         printf("Enter quantum: ");
//         scanf("%d", &quantum);

//         while (quantum <= 0)
//         {
//             printf("Quantum must be greater than zero!\n");
//             scanf("%d", &quantum);
//         }
//     }

//     msgq_id = msgget(MSGKEY, IPC_CREAT | 0666);
//     if (msgq_id == -1)
//     {
//         perror("msgget failed");
//         exit(1);
//     }

    

//     pid_t scheduler_pid = fork();
//     if (scheduler_pid < 0)
//     {
//         perror("fork scheduler failed");
//         exit(1);
//     }
//     if (scheduler_pid == 0)
//     {
//         char algoStr[10], quantumStr[10];
//         sprintf(algoStr, "%d", chosensched);
//         sprintf(quantumStr, "%d", quantum);

//         execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);
//         perror("execl scheduler failed");
//         exit(1);
//     }
//     pid_t clk_pid = fork();
//     if (clk_pid < 0)
//     {
//         perror("fork clk failed");
//         exit(1);
//     }
//     if (clk_pid == 0)
//     {
//         execl("./clk.out", "clk.out", NULL);
//         perror("execl clk failed");
//         exit(1);
//     }
//     initClk();

//     /* important: start from current clock - 1 */
//     int lastClk = getClk() - 1;

//     while (!isEmpty(processes))
//     {
//         int now = getClk();

//         if (now == lastClk)
//             continue;

//         lastClk = now;

//         while (!isEmpty(processes))
//         {
//             ProcessData *p = peekQueue(processes);

//             if (p != NULL && p->arrival_time <= now)
//             {
//                 ProcessData *sent = NULL;
//                 dequeue(processes, &sent);

//                 if (sent != NULL)
//                 {
//                     ProcessMessage msg;
//                     msg.mtype = 1;
//                     msg.isLast = 0;
//                     msg.p.id = sent->id;
//                     msg.p.arrival_time = sent->arrival_time;
//                     msg.p.runtime = sent->runtime;
//                     msg.p.priority = sent->priority;

//                     if (msgsnd(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0) == -1)
//                         perror("msgsnd failed");

//                     free(sent);
//                 }
//             }
//             else
//             {
//                 break;
//             }
//         }
//     }

//     ProcessMessage endMsg;
//     endMsg.mtype = 1;
//     endMsg.isLast = 1;

//     if (msgsnd(msgq_id, &endMsg, sizeof(ProcessMessage) - sizeof(long), 0) == -1)
//         perror("msgsnd end failed");

//     waitpid(scheduler_pid, NULL, 0);

//     destroyClk(true);
//     return 0;
// }
