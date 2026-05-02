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

    FILE *f = fopen(argv[1], "r");
    if (f == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }

    int id, arrival, runtime, priority, base, limit;
    char line[200];

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (sscanf(line, "%d %d %d %d %d %d",
                &id, &arrival, &runtime, &priority, &base, &limit) == 6)
        {
            ProcessData *p = malloc(sizeof(ProcessData));
            p->id = id;
            p->arrival_time = arrival;
            p->runtime = runtime;
            p->priority = priority;
            p->base = base;
            p->limit = limit;
            enqueue(processes, p);
        }
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
    if (chosensched == 2)
    {
        printf("Enter quantum: ");
        scanf("%d", &quantum);
        while (quantum <= 0)
        {
            printf("Quantum must be greater than zero!\n");
            scanf("%d", &quantum);
        }
    }

    int nruK = 1;
    if (chosensched == 2)
    {
        printf("Enter NRU R-bit reset timeout K (in quantums): ");
        scanf("%d", &nruK);
        while (nruK <= 0)
        {
            printf("K must be greater than zero!\n");
            scanf("%d", &nruK);
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

        char nruStr[10];
        sprintf(nruStr, "%d", nruK);

        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, nruStr, NULL);
        perror("execl scheduler failed");
        exit(1);
    }

    initClk();

    while (!isEmpty(processes))
    {
        int now = getClk();
        
        while (!isEmpty(processes)){
            ProcessData *p = peekQueue(processes);
            
            if (p->arrival_time <= now)
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
                msg.p.base         = p->base;
                msg.p.limit        = p->limit;
                if (msgsnd(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0) == -1)
                {
                    perror("msgsnd failed");
                }
                
                free(sent);
            }
            else
                break;
        }
        usleep(100000); // Sleep for 100ms to reduce CPU usage
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
