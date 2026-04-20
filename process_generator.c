#include "headers.h"

void clearResources(int);

int main(int argc, char * argv[])
{
    void clearResources(int);

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

    FILE *f = fopen(argv[1], "r");
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
    printf("2 - Highest Priority First (HPF)\n");
    printf("3 - Round Robin (RR)\n");
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
    // 3. Initiate and create the scheduler and clock processes.
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
}
