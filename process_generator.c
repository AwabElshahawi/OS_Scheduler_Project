#include "headers.h"

void clearResources(int);

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    CircularQueue *PCBs = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(PCBs);
    FILE *f;
    f = fopen(argv[1], "r");
    if (f == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    // 1. Read the input files.
    int id, arrival, runtime, priority;
    fscanf(f, "%*[^\n]\n");
    while (fscanf(f, "%d %d %d %d", &id, &arrival, &runtime, &priority) == 4)
    {
        PCB *p = (PCB *)malloc(sizeof(PCB));
        p->id = id;
        p->arrival_time = arrival;
        p->runtime = runtime;
        p->priority = priority;
        p->remaining_time = runtime;
        enqueue(PCBs, p);
    }
    fclose(f);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    printf("\nEnter a scheduling algorithm:\n");
    printf("1 - Preemptive Highest Priority First (HPF)\n");
    printf("2 - Highest Priority First (HPF)\n");
    printf("3 - Round Robin (RR)\n");
    printf("Please enter the number corresponding to your choice: ");

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
