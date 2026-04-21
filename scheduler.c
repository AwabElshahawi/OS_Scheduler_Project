#include "headers.h"
#include "priQueue.h"
#include "queue.h"
#include "PCBqueue.h"
#include <math.h>
volatile sig_atomic_t childFinished = 0;

void onChildFinished(int signum)
{
    childFinished = 1;
}
float avgWTA;
float avgWaiting;
float stdWTA;
float cpuUtilization;
float totalWTA = 0.0;
float totalWaiting = 0.0;
float totalWTA2 = 0.0;
int totalRuntimeAll = 0;
int totalProcesses = 0;
int lastFinishTime = 0;

void runHPF(int msgq_id);
void runRR(int msgq_id, int quantum);
// void runFCFS(int msgq_id); //TODO

int main(int argc, char * argv[])
{
    avgWTA=0.0;
    avgWaiting=0.0;
    stdWTA=0.0;
    cpuUtilization=0.0;
    totalWTA = 0.0;
    totalWaiting = 0.0;
    totalWTA2 = 0.0;
    totalRuntimeAll = 0;
    totalProcesses = 0;
    lastFinishTime = 0;
    initClk();
    int algo = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    signal(SIGUSR1, onChildFinished);   


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

        // case 3:
        //     /* FCFS if you have it */
        //     runFCFS(msgq_id);
        //     break;

        default:
            printf("Invalid algorithm number\n");
            break;
    }

    if (totalProcesses > 0)
    {
        avgWTA = totalWTA / totalProcesses;
        avgWaiting = totalWaiting / totalProcesses;
        stdWTA = sqrt((totalWTA2 / totalProcesses) - (avgWTA * avgWTA));
    }

    if (lastFinishTime > 0)
    {
        cpuUtilization = ((float) totalRuntimeAll / lastFinishTime) * 100.0;
    }
     
    FILE *perfFile = fopen("scheduler.perf", "w");
    if (perfFile == NULL)
    {
        perror("fopen scheduler.perf failed");
    }
    else
    {
        fprintf(perfFile, "CPU utilization = %.2f%%\n", cpuUtilization);
        fprintf(perfFile, "Avg WTA = %.2f\n", avgWTA);
        fprintf(perfFile, "Avg Waiting = %.2f\n", avgWaiting);
        fprintf(perfFile, "Std WTA = %.2f\n", stdWTA);
        fclose(perfFile);
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
    PCB *nextAfterSwitch = NULL;

    int allProcessesSent = 0;
    int lastClk = -1;

    int startedOnce = 0;
    int switching = 0;
    int switchEndTime = -1;

    while (1)
    {
        while (1)
        {
            ProcessMessage msg;
            int ret = msgrcv(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0, IPC_NOWAIT);

            if (ret == -1) break;
            
            if (msg.isLast)
            {
                allProcessesSent = 1;
            }
            else
            {
                totalRuntimeAll += msg.p.runtime;
                totalProcesses++;
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

                pcb->state = READY;
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
        
        int now = getClk() - 1;
        if (now == lastClk) { usleep(1000); continue; }
        lastClk = now;
        /* 2) if current process finished, finalize it */
    if (currentProcess != NULL)
    {
        int ran = now - currentProcess->last_start_time;
        int updatedRemaining = currentProcess->runtime 
                            - currentProcess->executed_time 
                            - ran;

        if (updatedRemaining <= 0)
        {
            /* we know it's done — block until it fully exits */
            int status;
            waitpid(currentProcess->pid, &status, 0);

            int finishTime = currentProcess->last_start_time 
                        + currentProcess->remaining_time;

            currentProcess->executed_time  += currentProcess->remaining_time;
            currentProcess->remaining_time  = 0;
            currentProcess->finish_time     = finishTime;
            currentProcess->TA              = finishTime - currentProcess->arrival_time;
            currentProcess->waiting_time    = currentProcess->TA - currentProcess->runtime;
            currentProcess->WTA             = (float)currentProcess->TA / currentProcess->runtime;
            currentProcess->state           = FINISHED;

            //perf
            totalWTA += currentProcess->WTA;
            totalWaiting += currentProcess->waiting_time;
            totalWTA2 += currentProcess->WTA * currentProcess->WTA;
            lastFinishTime = currentProcess->finish_time;

            fprintf(logFile,
                    "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                    finishTime,
                    currentProcess->id,
                    currentProcess->arrival_time,
                    currentProcess->runtime,
                    currentProcess->waiting_time,
                    currentProcess->TA,
                    currentProcess->WTA);
            fflush(logFile);

            free(currentProcess);
            currentProcess = NULL;
            childFinished  = 0;

            if (!isPriorityQueueEmpty(readyQueue))
            {
                switching       = 1;
                switchEndTime   = finishTime + 1;
                nextAfterSwitch = dequeuePriQueue(readyQueue);
            }
            continue;
        }
}
        /* 3) preemptive HPF: stop current process if a better one exists */
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
                currentProcess->state = STOPPED;

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

                /* 1-second context switch starts now */
                switching = 1;
                switchEndTime = now + 1;
                nextAfterSwitch = dequeuePriQueue(readyQueue);
                continue;
            }
        }

        /* 4) if context switch finished, start/resume next process */
        if (switching && now >= switchEndTime && nextAfterSwitch != NULL)
        {
            PCB *next = nextAfterSwitch;
            nextAfterSwitch = NULL;
            switching = 0;

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
                if (next->start_time == -1)
                    next->start_time = now;
                next->last_start_time = now;
                next->state = RUNNING;
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
                next->state = RUNNING;
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

        /* 5) first ever process starts immediately, no switch cost */
      if (currentProcess == NULL && !switching && !isPriorityQueueEmpty(readyQueue))
{
    PCB *next = dequeuePriQueue(readyQueue);
    if (next == NULL) continue;

    if (next->pid == -1)
    {
        pid_t pid = fork();
        if (pid < 0) { perror("fork failed"); free(next); continue; }
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
        next->state          = RUNNING;
        currentProcess       = next;
    }
    else
    {
        kill(next->pid, SIGCONT);
        next->last_start_time = now;
        next->state           = RUNNING;
        currentProcess        = next;
    }

    int wait = now - next->arrival_time - next->executed_time;
    if (wait < 0) wait = 0;

    fprintf(logFile,
            next->start_time == now
                ? "At time %d process %d started arr %d total %d remain %d wait %d\n"
                : "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
            now, next->id, next->arrival_time,
            next->runtime, next->remaining_time, wait);
    fflush(logFile);
}

        /* 6) exit when generator sent all processes and nothing remains */
        if (allProcessesSent &&
            currentProcess == NULL &&
            nextAfterSwitch == NULL &&
            !switching &&
            isPriorityQueueEmpty(readyQueue))
            break;
    }

    fclose(logFile);
    free(readyQueue);
}
void runRR(int msgq_id, int quantum)
{
    PCBCircularQueue *readyQueue = (PCBCircularQueue *)malloc(sizeof(PCBCircularQueue));
    if (readyQueue == NULL) { perror("malloc failed for readyQueue"); return; }
    initPCBQueue(readyQueue);

    FILE *logFile = fopen("scheduler.log", "w");
    if (logFile == NULL) { perror("fopen scheduler.log failed"); free(readyQueue); return; }

    fprintf(logFile, "#At time x process y state arr w total z remain y wait k\n");
    fflush(logFile);

    PCB *currentProcess  = NULL;
    PCB *nextAfterSwitch = NULL;

    int allProcessesSent = 0;
    int lastClk          = -1;
    int quantumStart     = -1;
    int switching        = 0;
    int switchEndTime    = -1;

    while (1)
    {
        /* drain messages every spin before checking clock */
        while (1)
        {
            ProcessMessage msg;
            int ret = msgrcv(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0, IPC_NOWAIT);
            if (ret == -1)
            {
                if (errno == ENOMSG) break;
                perror("msgrcv failed");
                break;
            }

            if (msg.isLast)
            {
                allProcessesSent = 1;
            }
            else
            {    totalRuntimeAll += msg.p.runtime;
                totalProcesses++;
                PCB *pcb = (PCB *)malloc(sizeof(PCB));
                if (pcb == NULL) { perror("malloc failed for PCB"); continue; }

                pcb->id              = msg.p.id;
                pcb->arrival_time    = msg.p.arrival_time;
                pcb->runtime         = msg.p.runtime;
                pcb->priority        = msg.p.priority;
                pcb->state           = READY;
                pcb->remaining_time  = msg.p.runtime;
                pcb->waiting_time    = 0;
                pcb->pid             = -1;
                pcb->start_time      = -1;
                pcb->finish_time     = -1;
                pcb->last_start_time = -1;
                pcb->executed_time   = 0;
                pcb->TA              = 0;
                pcb->WTA             = 0.0f;
                pcb->next            = NULL;
                pcb->prev            = NULL;

                enqueuePCB(readyQueue, pcb);
            }
        }

        int now = getClk() - 1;
        if (now == lastClk) { usleep(1000); continue; }
        lastClk = now;

        /* 2) check if current process finished by computing remaining time */
        if (currentProcess != NULL)
        {
            int ran              = now - currentProcess->last_start_time;
            int updatedRemaining = currentProcess->runtime
                                 - currentProcess->executed_time
                                 - ran;

            if (updatedRemaining <= 0)
            {
                /* finished — block until fully exited */
                int status;
                waitpid(currentProcess->pid, &status, 0);

                int finishTime = currentProcess->last_start_time
                               + currentProcess->remaining_time;

                currentProcess->executed_time  += currentProcess->remaining_time;
                currentProcess->remaining_time  = 0;
                currentProcess->finish_time     = finishTime;
                currentProcess->TA              = finishTime - currentProcess->arrival_time;
                currentProcess->waiting_time    = currentProcess->TA - currentProcess->runtime;
                currentProcess->WTA             = (float)currentProcess->TA / currentProcess->runtime;
                currentProcess->state           = FINISHED;
//perf
                totalWTA += currentProcess->WTA;
                totalWaiting += currentProcess->waiting_time;
                totalWTA2 += currentProcess->WTA * currentProcess->WTA;
                lastFinishTime = currentProcess->finish_time;

                fprintf(logFile,
                        "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        finishTime,
                        currentProcess->id,
                        currentProcess->arrival_time,
                        currentProcess->runtime,
                        currentProcess->waiting_time,
                        currentProcess->TA,
                        currentProcess->WTA);
                fflush(logFile);

                free(currentProcess);
                currentProcess = NULL;
                quantumStart   = -1;

                /* start switch overhead if someone is waiting */
                if (!isPCBQueueEmpty(readyQueue))
                {
                    PCB *next = NULL;
                    dequeuePCB(readyQueue, &next);
                    switching       = 1;
                    switchEndTime   = finishTime + 1;
                    nextAfterSwitch = next;
                }
                continue;
            }
        }

        /* 3) quantum expired and process still running */
        if (currentProcess != NULL && (now - quantumStart) >= quantum)
        {
            int ran = now - currentProcess->last_start_time;
            if (ran < 0) ran = 0;

            currentProcess->executed_time  += ran;
            currentProcess->remaining_time -= ran;
            if (currentProcess->remaining_time < 0)
                currentProcess->remaining_time = 0;

            if (!isPCBQueueEmpty(readyQueue))
            {
                /* others waiting — stop current and start switch */
                kill(currentProcess->pid, SIGSTOP);
                currentProcess->state = STOPPED;

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

                enqueuePCB(readyQueue, currentProcess);

                PCB *next = NULL;
                dequeuePCB(readyQueue, &next);

                currentProcess  = NULL;
                quantumStart    = -1;
                switching       = 1;
                switchEndTime   = now + 1;
                nextAfterSwitch = next;
                continue;
            }
            else
            {
                /* nobody else waiting — extend quantum, keep running */
                currentProcess->last_start_time = now;
                quantumStart = now;
            }
        }

        /* 4) context switch finished — dispatch next process */
        if (switching && now >= switchEndTime && nextAfterSwitch != NULL)
        {
            PCB *next       = nextAfterSwitch;
            nextAfterSwitch = NULL;
            switching       = 0;

            if (next->pid == -1)
            {
                pid_t pid = fork();
                if (pid < 0) { perror("fork failed"); free(next); continue; }
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
                next->state          = RUNNING;
                currentProcess       = next;
                quantumStart         = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now, next->id, next->arrival_time,
                        next->runtime, next->remaining_time, wait);
                fflush(logFile);
            }
            else
            {
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state           = RUNNING;
                currentProcess        = next;
                quantumStart          = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now, next->id, next->arrival_time,
                        next->runtime, next->remaining_time, wait);
                fflush(logFile);
            }
        }

        /* 5) CPU idle with no pending switch — dispatch immediately (no cost) */
        if (currentProcess == NULL && !switching && !isPCBQueueEmpty(readyQueue))
        {
            PCB *next = NULL;
            if (!dequeuePCB(readyQueue, &next) || next == NULL) continue;

            if (next->pid == -1)
            {
                pid_t pid = fork();
                if (pid < 0) { perror("fork failed"); free(next); continue; }
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
                next->state          = RUNNING;
                currentProcess       = next;
                quantumStart         = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now, next->id, next->arrival_time,
                        next->runtime, next->remaining_time, wait);
                fflush(logFile);
            }
            else
            {
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state           = RUNNING;
                currentProcess        = next;
                quantumStart          = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0) wait = 0;

                fprintf(logFile,
                        "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now, next->id, next->arrival_time,
                        next->runtime, next->remaining_time, wait);
                fflush(logFile);
            }
        }

        /* 6) exit condition */
        if (allProcessesSent &&
            currentProcess  == NULL &&
            nextAfterSwitch == NULL &&
            !switching &&
            isPCBQueueEmpty(readyQueue))
            break;
    }

    fclose(logFile);
    free(readyQueue);
}
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
    
   

