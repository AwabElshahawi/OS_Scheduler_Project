#include "headers.h"


int main(int argc, char * argv[])
{
    initClk();
    int algo = atoi(argv[1]);
    int quantum = atoi(argv[2]);



    int msgq_id = msgget(MSGKEY, 0666);
    if (msgq_id == -1)
    {
        perror("msgget failed");
        destroyClk(false);
        return 1;
    }

    switch (algo)
    {
        case 1:
            /* Preemptive HPF */
            runHPF(msgq_id);
            break;

        case 2:
            /* RR */
            runRR(msgq_id, quantum);
            break;

        case 3:
            /* FCFS if you have it */
            runFCFS(msgq_id);
            break;

        default:
            printf("Invalid algorithm number\n");
            break;
    }

    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    destroyClk(false);
    return 0;
}
    


  void runHPF(int msgq_id)
{
    priQueue *readyQueue = createPriQueue();
    if (readyQueue == NULL)
    {
        perror("createPriQueue failed");
        return;
    }

    PCB *currentProcess = NULL;
    int allProcessesSent = 0;
    int lastClk = -1;

    while (1)
    {
        int now = getClk();
        if (now == lastClk)
            continue;
        lastClk = now;

        /* 1) Receive all newly arrived processes */
        while (1)
        {
            ProcessMessage msg;
            int ret = msgrcv(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0, IPC_NOWAIT);

            if (ret == -1)
            {
                if (errno == ENOMSG)
                    break;

                perror("msgrcv failed");
                break;
            }

            if (msg.isLast)
            {
                allProcessesSent = 1;
            }
            else
            {
                PCB *pcb = (PCB *)malloc(sizeof(PCB));
                if (pcb == NULL)
                {
                    perror("malloc failed for PCB");
                    continue;
                }

                pcb->id = msg.p.id;
                pcb->arrival_time = msg.p.arrival_time;
                pcb->runtime = msg.p.runtime;
                pcb->priority = msg.p.priority;

                pcb->state = STATE_READY;
                pcb->remaining_time = msg.p.runtime;
                pcb->waiting_time = 0;

                pcb->pid = -1;

                pcb->start_time = -1;
                pcb->finish_time = -1;
                pcb->last_start_time = -1;

                pcb->executed_time = 0;

                pcb->TA = 0;
                pcb->WTA = 0.0f;

                pcb->next = NULL;
                pcb->prev = NULL;

                priQueueInsert(readyQueue, pcb);
            }
        }

        /* 2) If current process finished, finalize it */
        if (childFinished && currentProcess != NULL)
        {
            int status;
            waitpid(currentProcess->pid, &status, WNOHANG);

            int ran = now - currentProcess->last_start_time;
            if (ran < 0)
                ran = 0;

            currentProcess->executed_time += ran;
            currentProcess->remaining_time = 0;
            currentProcess->finish_time = now;
            currentProcess->TA = currentProcess->finish_time - currentProcess->arrival_time;
            currentProcess->waiting_time = currentProcess->TA - currentProcess->runtime;
            currentProcess->WTA = (float) currentProcess->TA / currentProcess->runtime;
            currentProcess->state = STATE_FINISHED;

            free(currentProcess);
            currentProcess = NULL;
            childFinished = 0;
        }

        /* 3) Preemptive HPF: stop current process if a better one exists */
        if (currentProcess != NULL && !isPriorityQueueEmpty(readyQueue))
        {
            PCB *top = peekPriQueue(readyQueue);

            /* smaller priority number = higher priority */
            if (top != NULL && top->priority < currentProcess->priority)
            {
                int ran = now - currentProcess->last_start_time;
                if (ran < 0)
                    ran = 0;

                currentProcess->executed_time += ran;
                currentProcess->remaining_time -= ran;

                if (currentProcess->remaining_time < 0)
                    currentProcess->remaining_time = 0;

                kill(currentProcess->pid, SIGSTOP);
                currentProcess->state = STATE_STOPPED;

                priQueueInsert(readyQueue, currentProcess);
                currentProcess = NULL;
            }
        }

        /* 4) If CPU is idle, dispatch highest-priority process */
        if (currentProcess == NULL && !isPriorityQueueEmpty(readyQueue))
        {
            PCB *next = dequeuePriQueue(readyQueue);
            if (next == NULL)
                continue;

            /* First run */
            if (next->pid == -1)
            {
                pid_t pid = fork();

                if (pid < 0)
                {
                    perror("fork failed");
                    free(next);
                    continue;
                }

                if (pid == 0)
                {
                    char remStr[20];
                    sprintf(remStr, "%d", next->remaining_time);

                    execl("./process.out", "process.out", remStr, NULL);

                    perror("execl failed");
                    exit(1);
                }

                next->pid = pid;
                next->start_time = now;
                next->last_start_time = now;
                next->state = STATE_RUNNING;
                currentProcess = next;
            }
            /* Resume stopped process */
            else
            {
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state = STATE_RUNNING;
                currentProcess = next;
            }
        }

        /* 5) Exit when generator sent all processes and nothing remains */
        if (allProcessesSent && currentProcess == NULL && isPriorityQueueEmpty(readyQueue))
            break;
    }

    free(readyQueue);
}


    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
   

