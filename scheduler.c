#include "headers.h"
#include "priQueue.h"
#include "queue.h"
#include "PCBqueue.h"
#include <math.h>
#include "MMU.h"

static void int_to_binary_str(int val, char *buf, int bits)
{
    buf[bits] = '\0';
    for (int i = bits - 1; i >= 0; i--)
    {
        buf[i] = (val & 1) ? '1' : '0';
        val >>= 1;
    }
}


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
void runRR(int msgq_id, int quantum, int nru_reset_quantums);
void load_process_requests(PCB *pcb);

static int parse_request_address(const char *address_text)
{
    for (int i = 0; address_text[i] != '\0'; i++)
    {
        if (address_text[i] != '0' && address_text[i] != '1')
            return (int)strtol(address_text, NULL, 10);
    }

    return (int)strtol(address_text, NULL, 2);
}

typedef struct BlockedNode
{
    PCB *pcb;
    int fault_time;
    int finish_time;
    int virtual_page;
    int frame;
    char op;
    struct BlockedNode *next;
} BlockedNode;

static void add_blocked_process(BlockedNode **blocked_list, PCB *pcb, int fault_time, int finish_time, int virtual_page, int frame, char op)
{
    BlockedNode *node = (BlockedNode *)malloc(sizeof(BlockedNode));
    if (node == NULL)
    {
        perror("malloc failed for blocked process");
        return;
    }

    node->pcb = pcb;
    node->fault_time = fault_time;
    node->finish_time = finish_time;
    node->virtual_page = virtual_page;
    node->frame = frame;
    node->op = op;
    node->next = NULL;

    if (*blocked_list == NULL || finish_time < (*blocked_list)->finish_time)
    {
        node->next = *blocked_list;
        *blocked_list = node;
        return;
    }

    BlockedNode *cur = *blocked_list;
    while (cur->next != NULL && cur->next->finish_time <= finish_time)
        cur = cur->next;

    node->next = cur->next;
    cur->next = node;
}

static void release_finished_disk_requests(BlockedNode **blocked_list, PCBCircularQueue *readyQueue, MMU *mmu, int now)
{
    while (*blocked_list != NULL && (*blocked_list)->finish_time <= now)
    {
        BlockedNode *done = *blocked_list;
        *blocked_list = done->next;

        if (mmu_complete_page_load(mmu, done->pcb, done->virtual_page, done->op, done->frame, now) == -1)
        {
            printf("No frame available for faulted page of process %d\n", done->pcb->id);
            free(done);
            continue;
        }

        done->pcb->state = READY;
        enqueuePCB(readyQueue, done->pcb);
        free(done);
    }
}

static void free_blocked_list(BlockedNode *blocked_list)
{
    while (blocked_list != NULL)
    {
        BlockedNode *next = blocked_list->next;
        free(blocked_list);
        blocked_list = next;
    }
}

static int prepare_process_memory(MMU *mmu, PCB *pcb, int now)
{
    if (pcb->page_table_frame != -1)
        return 1;

    pcb->page_table_frame = mmu_allocate_page_table(mmu, pcb->id);
    if (pcb->page_table_frame == -1)
    {
        printf("No frame available for page table of process %d\n", pcb->id);
        return 0;
    }

    if (mmu_load_initial_page(mmu, pcb, now) == -1)
    {
        printf("No frame available for initial page of process %d\n", pcb->id);
        return 0;
    }

    return 1;
}

int main(int argc, char * argv[])
{
    avgWTA = 0.0;
    avgWaiting = 0.0;
    stdWTA = 0.0;
    cpuUtilization = 0.0;
    totalWTA = 0.0;
    totalWaiting = 0.0;
    totalWTA2 = 0.0;
    totalRuntimeAll = 0;
    totalProcesses = 0;
    lastFinishTime = 0;

    initClk();

    int algo = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    int nru_reset_quantums = atoi(argv[3]);

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
            runHPF(msgq_id);
            break;

        case 2:
            runRR(msgq_id, quantum, nru_reset_quantums);
            break;

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
    int switching = 0;
    int switchEndTime = -1;

    while (1)
    {
        while (1)
        {
            ProcessMessage msg;
            int ret = msgrcv(msgq_id, &msg, sizeof(ProcessMessage) - sizeof(long), 0, IPC_NOWAIT);

            if (ret == -1)
                break;

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
        if (now == lastClk)
        {
            usleep(1000);
            continue;
        }
        lastClk = now;

        if (currentProcess != NULL)
        {
            int ran = now - currentProcess->last_start_time;
            int updatedRemaining = currentProcess->runtime - currentProcess->executed_time - ran;

            if (updatedRemaining <= 0)
            {
                int status;
                waitpid(currentProcess->pid, &status, 0);

                int finishTime = currentProcess->last_start_time + currentProcess->remaining_time;

                currentProcess->executed_time += currentProcess->remaining_time;
                currentProcess->remaining_time = 0;
                currentProcess->finish_time = finishTime;
                currentProcess->TA = finishTime - currentProcess->arrival_time;
                currentProcess->waiting_time = currentProcess->TA - currentProcess->runtime;
                currentProcess->WTA = (float) currentProcess->TA / currentProcess->runtime;
                currentProcess->state = FINISHED;

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
                childFinished = 0;

                if (!isPriorityQueueEmpty(readyQueue))
                {
                    switching = 1;
                    switchEndTime = finishTime + 1;
                    nextAfterSwitch = dequeuePriQueue(readyQueue);
                }
                continue;
            }
        }

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
                if (wait < 0)
                    wait = 0;

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

                switching = 1;
                switchEndTime = now + 1;
                nextAfterSwitch = dequeuePriQueue(readyQueue);
                continue;
            }
        }

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
                if (wait < 0)
                    wait = 0;

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
                if (wait < 0)
                    wait = 0;

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

        if (currentProcess == NULL && !switching && !isPriorityQueueEmpty(readyQueue))
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
                    exit(1);
                }

                next->pid = pid;
                next->start_time = now;
                next->last_start_time = now;
                next->state = RUNNING;
                currentProcess = next;
            }
            else
            {
                kill(next->pid, SIGCONT);
                next->last_start_time = now;
                next->state = RUNNING;
                currentProcess = next;
            }

            int wait = now - next->arrival_time - next->executed_time;
            if (wait < 0)
                wait = 0;

            fprintf(logFile,
                    next->start_time == now
                        ? "At time %d process %d started arr %d total %d remain %d wait %d\n"
                        : "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                    now,
                    next->id,
                    next->arrival_time,
                    next->runtime,
                    next->remaining_time,
                    wait);
            fflush(logFile);
        }

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

void runRR(int msgq_id, int quantum, int nru_reset_quantums)
{
    PCBCircularQueue *readyQueue = (PCBCircularQueue *)malloc(sizeof(PCBCircularQueue));
    if (readyQueue == NULL)
    {
        perror("malloc failed for readyQueue");
        return;
    }
    initPCBQueue(readyQueue);

    FILE *logFile = fopen("scheduler.log", "w");
    FILE *memoryLog = fopen("memory.log", "w");

    if (logFile == NULL || memoryLog == NULL)
    {
        perror("fopen log failed");
        if (logFile) fclose(logFile);
        if (memoryLog) fclose(memoryLog);
        free(readyQueue);
        return;
    }

    MMU mmu;
    mmu_init(&mmu, nru_reset_quantums, memoryLog);

    fprintf(logFile, "#At time x process y state arr w total z remain y wait k\n");
    fflush(logFile);

    PCB *currentProcess  = NULL;
    PCB *nextAfterSwitch = NULL;

    BlockedNode *blocked_list = NULL;

    int allProcessesSent = 0;
    int lastClk = -1;
    int quantumStart = -1;
    int switching = 0;
    int switchEndTime = -1;

    while (1)
    {
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
                pcb->base = msg.p.base;
                pcb->limit = msg.p.limit;

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

                pcb->page_table = (PageTableEntry *)malloc(sizeof(PageTableEntry) * pcb->limit);
                if (pcb->page_table == NULL)
                {
                    perror("malloc failed for page table");
                    free(pcb);
                    continue;
                }

                for (int i = 0; i < pcb->limit; i++)
                {
                    pcb->page_table[i].frame_number = -1;
                    pcb->page_table[i].present = 0;
                    pcb->page_table[i].referenced = 0;
                    pcb->page_table[i].modified = 0;
                }

                pcb->page_table_frame = -1;

                pcb->requests = NULL;
                pcb->request_count = 0;
                pcb->next_request_index = 0;
                load_process_requests(pcb);

                enqueuePCB(readyQueue, pcb);
            }
        }

        int now = getClk() - 1;
        if (now == lastClk)
        {
            usleep(1000);
            continue;
        }
        lastClk = now;

        release_finished_disk_requests(&blocked_list, readyQueue, &mmu, now);

        if (currentProcess != NULL)
        {
            int ran = now - currentProcess->last_start_time;
            int updatedRemaining = currentProcess->runtime - currentProcess->executed_time - ran;

            if (updatedRemaining <= 0)
            {
                int status;
                waitpid(currentProcess->pid, &status, 0);

                int finishTime = currentProcess->last_start_time + currentProcess->remaining_time;

                currentProcess->executed_time += currentProcess->remaining_time;
                currentProcess->remaining_time = 0;
                currentProcess->finish_time = finishTime;
                currentProcess->TA = finishTime - currentProcess->arrival_time;
                currentProcess->waiting_time = currentProcess->TA - currentProcess->runtime;
                currentProcess->WTA = (float) currentProcess->TA / currentProcess->runtime;
                currentProcess->state = FINISHED;

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

                mmu_free_process(&mmu, currentProcess);

                if (currentProcess->requests != NULL)
                    free(currentProcess->requests);

                free(currentProcess);
                currentProcess = NULL;
                quantumStart = -1;

                if (!isPCBQueueEmpty(readyQueue))
                {
                    PCB *next = NULL;
                    dequeuePCB(readyQueue, &next);

                    switching = 1;
                    switchEndTime = finishTime + 1;
                    nextAfterSwitch = next;
                }

                continue;
            }
        }

        if (currentProcess != NULL)
        {
            int cpuConsumed = currentProcess->executed_time + (now - currentProcess->last_start_time);

            while (currentProcess->next_request_index < currentProcess->request_count &&
                   currentProcess->requests[currentProcess->next_request_index].time <= cpuConsumed)
            {
                MemoryRequest req = currentProcess->requests[currentProcess->next_request_index];

                int accessResult = mmu_access_memory(&mmu, currentProcess, req.address, req.op, now);
                currentProcess->next_request_index++;

                if (accessResult == 0)
                {
                    int ran = now - currentProcess->last_start_time;
                    if (ran < 0)
                        ran = 0;

                    currentProcess->executed_time += ran;
                    currentProcess->remaining_time -= ran;
                    if (currentProcess->remaining_time < 0)
                        currentProcess->remaining_time = 0;

                    kill(currentProcess->pid, SIGSTOP);
                    currentProcess->state = BLOCKED;

                    int disk_cost = 10;
                    int frame = mmu_reserve_page_frame(&mmu, currentProcess, req.address / PAGE_SIZE_BYTES, req.op, &disk_cost);
                    if (frame == -1)
                    {
                        printf("No frame available for faulted page of process %d\n", currentProcess->id);
                        disk_cost = 10;
                    }

                    add_blocked_process(&blocked_list, currentProcess, now, now + disk_cost, req.address / PAGE_SIZE_BYTES, frame, req.op);

                    mmu_account_quantum(&mmu);
                    currentProcess = NULL;
                    quantumStart = -1;

                    if (!isPCBQueueEmpty(readyQueue))
                    {
                        PCB *next = NULL;
                        dequeuePCB(readyQueue, &next);

                        switching = 1;
                        switchEndTime = now + 1;
                        nextAfterSwitch = next;
                    }

                    break;
                }
            }
        }

        if (currentProcess != NULL && (now - quantumStart) >= quantum)
        {
            int ran = now - currentProcess->last_start_time;
            if (ran < 0)
                ran = 0;

            currentProcess->executed_time += ran;
            currentProcess->remaining_time -= ran;
            if (currentProcess->remaining_time < 0)
                currentProcess->remaining_time = 0;

            mmu_account_quantum(&mmu);

            if (!isPCBQueueEmpty(readyQueue))
            {
                kill(currentProcess->pid, SIGSTOP);
                currentProcess->state = STOPPED;

                int wait = now - currentProcess->arrival_time - currentProcess->executed_time;
                if (wait < 0)
                    wait = 0;

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

                currentProcess = NULL;
                quantumStart = -1;
                switching = 1;
                switchEndTime = now + 1;
                nextAfterSwitch = next;

                continue;
            }
            else
            {
                currentProcess->last_start_time = now;
                quantumStart = now;
            }
        }

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

                if (!prepare_process_memory(&mmu, next, now))
                {
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    free(next);
                    continue;
                }

                next->pid = pid;
                next->start_time = now;
                next->last_start_time = now;
                next->state = RUNNING;
                currentProcess = next;
                quantumStart = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0)
                    wait = 0;

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
                quantumStart = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0)
                    wait = 0;

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

        if (currentProcess == NULL && !switching && !isPCBQueueEmpty(readyQueue))
        {
            PCB *next = NULL;
            if (!dequeuePCB(readyQueue, &next) || next == NULL)
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

                if (!prepare_process_memory(&mmu, next, now))
                {
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    free(next);
                    continue;
                }

                next->pid = pid;
                next->start_time = now;
                next->last_start_time = now;
                next->state = RUNNING;
                currentProcess = next;
                quantumStart = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0)
                    wait = 0;

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
                quantumStart = now;

                int wait = now - next->arrival_time - next->executed_time;
                if (wait < 0)
                    wait = 0;

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

        if (allProcessesSent &&
            currentProcess == NULL &&
            nextAfterSwitch == NULL &&
            blocked_list == NULL &&
            !switching &&
            isPCBQueueEmpty(readyQueue))
        {
            break;
        }
    }

    fclose(logFile);
    fclose(memoryLog);
    free_blocked_list(blocked_list);
    free(readyQueue);
}

void load_process_requests(PCB *pcb)
{
    char filename[50];
    sprintf(filename, "requests%d.txt", pcb->id);

    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        pcb->request_count = 0;
        pcb->requests = NULL;
        return;
    }

    int capacity = 10;
    pcb->requests = malloc(sizeof(MemoryRequest) * capacity);
    pcb->request_count = 0;

    char line[100];

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        int time;
        char addressBinary[50];
        char op;

        if (sscanf(line, "%d %s %c", &time, addressBinary, &op) == 3)
        {
            if (pcb->request_count == capacity)
            {
                capacity *= 2;
                pcb->requests = realloc(pcb->requests, sizeof(MemoryRequest) * capacity);
            }

            int address = parse_request_address(addressBinary);

            pcb->requests[pcb->request_count].time = time;
            pcb->requests[pcb->request_count].address = address;
            pcb->requests[pcb->request_count].op = op;
            pcb->request_count++;
        }
    }

    fclose(f);
    pcb->next_request_index = 0;
}
