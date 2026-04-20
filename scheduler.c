#include "headers.h"
#include "priQueue.h"
#include "queue.h"

void runHPF(int msgq_id);
void runRR(int msgq_id, int quantum);
// void runFCFS(int msgq_id); //TODO

int main(int argc, char * argv[])
{
    initClk();
    int algo = atoi(argv[1]);
    int quantum = atoi(argv[2]);



    int msgq_id = msgget(SHKEY, 0666);
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

        // case 3:
        //     /* FCFS if you have it */
        //     runFCFS(msgq_id);
        //     break;

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

    FILE *logFile = fopen("scheduler.log", "w");
    if (logFile == NULL)
    {
        perror("fopen scheduler.log failed");
        free(readyQueue);
        return;
    }

    fprintf(logFile, "#At time x process y state arr w total z remain y wait k\n");
    fflush(logFile);

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
        // if (childFinished && currentProcess != NULL)
        if (currentProcess != NULL)
        {
            // int status;
            // waitpid(currentProcess->pid, &status, WNOHANG);
            int status;
            pid_t result = waitpid(currentProcess->pid, &status, WNOHANG);

          if (result == currentProcess->pid && WIFEXITED(status))
{
         int ran = now - currentProcess->last_start_time;
        if (ran < 0) ran = 0;

        currentProcess->executed_time  += ran;
        currentProcess->remaining_time  = 0;
        currentProcess->finish_time     = now;
        currentProcess->TA              = currentProcess->finish_time - currentProcess->arrival_time;
        currentProcess->waiting_time    = currentProcess->TA - currentProcess->runtime;
        currentProcess->WTA             = (float)currentProcess->TA / currentProcess->runtime;
        currentProcess->state           = STATE_FINISHED;

        fprintf(logFile,
                "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                now,
                currentProcess->id,
                currentProcess->arrival_time,
                currentProcess->runtime,
                currentProcess->waiting_time,
                currentProcess->TA,
                currentProcess->WTA);
        fflush(logFile);

        free(currentProcess);      // ✅ one free, at the very end
        currentProcess = NULL;
    }
                // childFinished = 0;
        }

        /* 3) Preemptive HPF: stop current process if a better one exists */
        if (currentProcess != NULL && !isPriorityQueueEmpty(readyQueue))
        {
            PCB *top = peekPriQueue(readyQueue);

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

                int wait = now - currentProcess->arrival_time - currentProcess->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        now,
                        currentProcess->id,
                        currentProcess->arrival_time,
                        currentProcess->runtime,
                        currentProcess->remaining_time,
                        wait);
                fflush(logFile);

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

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now,
                        next->id,
                        next->arrival_time,
                        next->runtime,
                        next->remaining_time,
                        wait);
                fflush(logFile);
            }
            else
            {
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state = STATE_RUNNING;
                currentProcess = next;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now,
                        next->id,
                        next->arrival_time,
                        next->runtime,
                        next->remaining_time,
                        wait);
                fflush(logFile);
            }
        }

        /* 5) Exit when generator sent all processes and nothing remains */
        if (allProcessesSent && currentProcess == NULL && isPriorityQueueEmpty(readyQueue))
            break;
    }

    fclose(logFile);
    free(readyQueue);
}
  
void runRR(int msgq_id, int quantum)
{
    CircularQueue *readyQueue; 
    initQueue(readyQueue);
    PCB *currentProcess = NULL;
    int allProcessesSent = 0;
    int lastClk = -1;
    int quantumStart = -1;

    while (1)
    {
        int now = getClk();
        if (now == lastClk)
            continue;
        lastClk = now;

        while (1)
        {
            ProcessMessage msg;
            int ret = msgrcv(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0, IPC_NOWAIT);

            if (ret == -1)
            {
                if (errno == ENOMSG)
                    break;
                break;
            }

            if (msg.isLast)
            {
                allProcessesSent = 1;
            }
            else
            {
                PCB *pcb = (PCB *)malloc(sizeof(PCB));

                pcb->id             = msg.p.id;
                pcb->arrival_time   = msg.p.arrival_time;
                pcb->runtime        = msg.p.runtime;
                pcb->priority       = msg.p.priority;
                pcb->remaining_time = msg.p.runtime;

                pcb->waiting_time    = 0;
                pcb->pid             = -1;
                pcb->start_time      = -1;
                pcb->finish_time     = -1;
                pcb->last_start_time = -1;
                pcb->executed_time   = 0;
                pcb->TA              = 0;
                pcb->WTA             = 0;
                pcb->state           = STATE_READY;
                pcb->next            = NULL;

                enqueue(readyQueue, pcb);
            }
        }

        // if (childFinished && currentProcess != NULL)
        if (currentProcess != NULL)
        {
            int status;
            waitpid(currentProcess->pid, &status, WNOHANG);

            int ran = now - currentProcess->last_start_time;
            if (ran < 0) ran = 0;

            currentProcess->executed_time  += ran;
            currentProcess->remaining_time  = 0;
            currentProcess->finish_time     = now;
            currentProcess->TA              = currentProcess->finish_time - currentProcess->arrival_time;
            currentProcess->waiting_time    = currentProcess->TA - currentProcess->runtime;
            currentProcess->WTA             = (float)currentProcess->TA / currentProcess->runtime;
            currentProcess->state           = STATE_FINISHED;

            // log finished event here
            free(currentProcess);
            currentProcess = NULL;
            quantumStart   = -1;
            // childFinished  = 0;
        }

        /* 3) RR preemption: quantum expired → stop current, push to back of queue */
        if (currentProcess != NULL && (now - quantumStart) >= quantum)
        {
            int ran = now - currentProcess->last_start_time;
            if (ran < 0) ran = 0;

            currentProcess->executed_time  += ran;
            currentProcess->remaining_time -= ran;
            if (currentProcess->remaining_time < 0)
                currentProcess->remaining_time = 0;

            kill(currentProcess->pid, SIGSTOP);
            currentProcess->state = STATE_STOPPED;

            // log stopped event here

            enqueue(readyQueue, currentProcess);   // goes to BACK of queue
            currentProcess = NULL;
            quantumStart   = -1;
        }

        /* 4) CPU idle → dispatch next from front of queue */
        if (currentProcess == NULL && !isEmpty(readyQueue))
        {
            PCB *next = dequeue(readyQueue);

            if (next->pid == -1)
            {
                // first time running this process → fork it
                pid_t pid = fork();

                if (pid == 0)
                {
                    char remStr[20];
                    sprintf(remStr, "%d", next->remaining_time);
                    execl("./process.out", "process.out", remStr, NULL);
                    exit(1);
                }

                next->pid            = pid;
                next->start_time     = now;
                next->last_start_time = now;
                next->state          = STATE_RUNNING;

                // log started event here
            }
            else
            {
                // process was stopped before → resume it
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state           = STATE_RUNNING;

                // log resumed event here
            }

            currentProcess = next;
            quantumStart   = now;    // reset quantum clock
        }

        /* 5) stop condition: all processes sent, nothing running, queue empty */
        if (allProcessesSent && currentProcess == NULL && isEmpty(readyQueue))
            break;
    }

    free(readyQueue);
}
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
    
   

