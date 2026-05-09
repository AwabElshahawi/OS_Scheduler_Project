#ifndef MMU_H
#define MMU_H

#include "headers.h"

#define RAM_SIZE_BYTES 512
#define PAGE_SIZE_BYTES 16
#define FRAME_COUNT (RAM_SIZE_BYTES / PAGE_SIZE_BYTES)
#define MAX_VPAGES_PER_PROCESS 64

typedef enum FrameType {
    FRAME_FREE = 0,
    FRAME_PAGE_TABLE,
    FRAME_RESERVED,
    FRAME_DATA
} FrameType;

typedef struct FrameInfo {
    int frame_number;
    FrameType type;
    int owner_pid;
    int virtual_page;
    int referenced;
    int modified;
    PageTableEntry *owner_page_table;
} FrameInfo;

typedef struct MMU {
    FrameInfo frames[FRAME_COUNT];
    int nru_reset_quantums;
    int elapsed_quantums;
    FILE *memory_log;
} MMU;

void mmu_init(MMU *mmu, int nru_reset_quantums, FILE *memory_log);
int mmu_allocate_frame(MMU *mmu);
int mmu_select_nru_victim(MMU *mmu);
void mmu_clear_referenced_bits(MMU *mmu);
void mmu_account_quantum(MMU *mmu);

int mmu_allocate_page_table(MMU *mmu, int pid);
int mmu_load_initial_page(MMU *mmu, PCB *pcb, int now);
int mmu_handle_page_fault(MMU *mmu, PCB *pcb, int virtual_address, char op, int now);
void mmu_free_process(MMU *mmu, PCB *pcb);
void mmu_invalidate_owner_page(MMU *mmu, int victim_frame, PCB **all_pcbs, int pcb_count);
void mmu_free_frame(MMU *mmu, int frame);
int mmu_reserve_page_frame(MMU *mmu, PCB *pcb, int virtual_page, char op, int *disk_ticks);
int mmu_complete_page_load(MMU *mmu, PCB *pcb, int virtual_page, char op, int frame, int now);
int mmu_load_page(MMU *mmu, PCB *pcb, int virtual_page, char op, int now, int *disk_ticks);
int mmu_access_memory(MMU *mmu, PCB *pcb, int virtual_address, char op, int now);

#endif
