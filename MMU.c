#include "MMU.h"

static int frame_nru_class(const FrameInfo *f)
{
    if (f->referenced == 0 && f->modified == 0) return 0;
    if (f->referenced == 0 && f->modified == 1) return 1;
    if (f->referenced == 1 && f->modified == 0) return 2;
    return 3;
}

void mmu_init(MMU *mmu, int nru_reset_quantums, FILE *memory_log)
{
    mmu->nru_reset_quantums = (nru_reset_quantums > 0) ? nru_reset_quantums : 1;
    mmu->elapsed_quantums = 0;
    mmu->memory_log = memory_log;

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        mmu->frames[i].frame_number = i;
        mmu->frames[i].type = FRAME_FREE;
        mmu->frames[i].owner_pid = -1;
        mmu->frames[i].virtual_page = -1;
        mmu->frames[i].referenced = 0;
        mmu->frames[i].modified = 0;
    }
}

int mmu_allocate_frame(MMU *mmu)
{
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (mmu->frames[i].type == FRAME_FREE)
            return i;
    }
    return -1;
}

int mmu_select_nru_victim(MMU *mmu)
{
    int best_class = 5;
    int victim = -1;

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (mmu->frames[i].type != FRAME_DATA)
            continue;

        int current_class = frame_nru_class(&mmu->frames[i]);
        if (current_class < best_class)
        {
            best_class = current_class;
            victim = i;
            if (best_class == 0)
                break;
        }
    }

    return victim;
}

void mmu_clear_referenced_bits(MMU *mmu)
{
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (mmu->frames[i].type == FRAME_DATA)
            mmu->frames[i].referenced = 0;
    }
}

void mmu_account_quantum(MMU *mmu)
{
    mmu->elapsed_quantums++;
    if (mmu->elapsed_quantums >= mmu->nru_reset_quantums)
    {
        mmu_clear_referenced_bits(mmu);
        mmu->elapsed_quantums = 0;
    }
}
