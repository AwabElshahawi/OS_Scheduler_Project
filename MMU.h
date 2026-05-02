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
    FRAME_DATA
} FrameType;

typedef struct PageTableEntry {
    int frame_number;
    int present;
    int referenced;
    int modified;
} PageTableEntry;

typedef struct FrameInfo {
    int frame_number;
    FrameType type;
    int owner_pid;
    int virtual_page;
    int referenced;
    int modified;
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

#endif
