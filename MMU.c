#include "MMU.h"

static void int_to_binary_str(int val, char *buf, int bits)
{
    if (val == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    int pos = 0;
    int started = 0;

    for (int i = bits - 1; i >= 0; i--)
    {
        int bit = (val >> i) & 1;
        if (bit)
            started = 1;
        if (started)
            buf[pos++] = bit ? '1' : '0';
    }

    buf[pos] = '\0';
}

static int frame_nru_class(const FrameInfo *f)
{
    if (f->referenced == 0 && f->modified == 0) return 0;
    if (f->referenced == 0 && f->modified == 1) return 1;
    if (f->referenced == 1 && f->modified == 0) return 2;
    return 3;
}

static int mmu_is_nru_candidate(const FrameInfo *f)
{
    return f->type == FRAME_DATA &&
           f->owner_page_table != NULL &&
           f->owner_pid != -1 &&
           f->virtual_page >= 0;
}

static void mmu_log_swap_out(MMU *mmu, int frame)
{
    fprintf(mmu->memory_log, "Swapping out page %d to disk\n", frame);
    fflush(mmu->memory_log);
}

static void mmu_log_frame_allocated(MMU *mmu, int frame)
{
    fprintf(mmu->memory_log, "Free Physical page %d allocated\n", frame);
    fflush(mmu->memory_log);
}

static void mmu_invalidate_frame_owner(MMU *mmu, int frame)
{
    if (mmu->frames[frame].owner_page_table != NULL)
    {
        int old_vpage = mmu->frames[frame].virtual_page;
        mmu->frames[frame].owner_page_table[old_vpage].present = 0;
        mmu->frames[frame].owner_page_table[old_vpage].frame_number = -1;
        mmu->frames[frame].owner_page_table[old_vpage].referenced = 0;
        mmu->frames[frame].owner_page_table[old_vpage].modified = 0;
    }
}

static int mmu_prepare_frame_for_data_page(MMU *mmu, int *disk_ticks, int log_events)
{
    int frame = mmu_allocate_frame(mmu);

    if (frame != -1)
    {
        if (log_events)
            mmu_log_frame_allocated(mmu, frame);
        if (disk_ticks) *disk_ticks = 10;
        return frame;
    }

    frame = mmu_select_nru_victim(mmu);
    if (frame == -1)
        return -1;

    if (log_events)
        mmu_log_swap_out(mmu, frame);

    if (mmu->frames[frame].modified)
    {
        if (disk_ticks) *disk_ticks = 20;
    }
    else
    {
        if (disk_ticks) *disk_ticks = 10;
    }

    mmu_invalidate_frame_owner(mmu, frame);
    return frame;
}

static int mmu_map_data_page(MMU *mmu, PCB *pcb, int virtual_page, char op, int frame)
{
    if (virtual_page < 0 || virtual_page >= pcb->limit)
        return -1;

    mmu->frames[frame].type = FRAME_DATA;
    mmu->frames[frame].owner_pid = pcb->id;
    mmu->frames[frame].virtual_page = virtual_page;
    mmu->frames[frame].referenced = 1;
    mmu->frames[frame].modified = (op == 'w');
    mmu->frames[frame].owner_page_table = pcb->page_table;

    pcb->page_table[virtual_page].frame_number = frame;
    pcb->page_table[virtual_page].present = 1;
    pcb->page_table[virtual_page].referenced = 1;
    pcb->page_table[virtual_page].modified = (op == 'w');

    return frame;
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
        mmu->frames[i].owner_page_table = NULL;
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
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (!mmu_is_nru_candidate(&mmu->frames[i]))
            continue;
        int c = frame_nru_class(&mmu->frames[i]);
        if (c < best_class)
            best_class = c;
        if (best_class == 0)
            break;
    }

    if (best_class == 5)
        return -1;

    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (!mmu_is_nru_candidate(&mmu->frames[i]))
            continue;
        if (frame_nru_class(&mmu->frames[i]) == best_class)
            return i;
    }

    return -1;
}

void mmu_clear_referenced_bits(MMU *mmu)
{
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        if (mmu->frames[i].type == FRAME_DATA)
        {
            mmu->frames[i].referenced = 0;
            if (mmu->frames[i].owner_page_table != NULL)
            {
                int vpage = mmu->frames[i].virtual_page;
                mmu->frames[i].owner_page_table[vpage].referenced = 0;
            }
        }
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
void mmu_free_frame(MMU *mmu, int frame)
{
    mmu->frames[frame].type = FRAME_FREE;
    mmu->frames[frame].owner_pid = -1;
    mmu->frames[frame].virtual_page = -1;
    mmu->frames[frame].referenced = 0;
    mmu->frames[frame].modified = 0;
    mmu->frames[frame].owner_page_table = NULL;
}
int mmu_allocate_page_table(MMU *mmu, int pid)
{
    int frame = mmu_allocate_frame(mmu);
    int was_free_frame = (frame != -1);

    if (frame == -1)
    {
        frame = mmu_select_nru_victim(mmu);
        if (frame == -1)
            return -1;

        mmu_log_swap_out(mmu, frame);

        mmu_invalidate_frame_owner(mmu, frame);
    }

    mmu->frames[frame].type = FRAME_PAGE_TABLE;
    mmu->frames[frame].owner_pid = pid;
    mmu->frames[frame].virtual_page = -1;
    mmu->frames[frame].referenced = 0;
    mmu->frames[frame].modified = 0;
    mmu->frames[frame].owner_page_table = NULL;

    if (was_free_frame)
        mmu_log_frame_allocated(mmu, frame);

    return frame;
}

int mmu_reserve_page_frame(MMU *mmu, PCB *pcb, int virtual_page, char op, int *disk_ticks)
{
    int frame = mmu_prepare_frame_for_data_page(mmu, disk_ticks, 1);
    if (frame == -1)
        return -1;

    mmu->frames[frame].type = FRAME_RESERVED;
    mmu->frames[frame].owner_pid = pcb->id;
    mmu->frames[frame].virtual_page = virtual_page;
    mmu->frames[frame].referenced = 0;
    mmu->frames[frame].modified = (op == 'w');
    mmu->frames[frame].owner_page_table = pcb->page_table;

    fflush(mmu->memory_log);
    return frame;
}

int mmu_complete_page_load(MMU *mmu, PCB *pcb, int virtual_page, char op, int frame, int now)
{
    if (frame < 0 || frame >= FRAME_COUNT || mmu->frames[frame].type != FRAME_RESERVED)
        return -1;

    if (mmu_map_data_page(mmu, pcb, virtual_page, op, frame) == -1)
        return -1;

    int disk_address = pcb->base + virtual_page;

    fprintf(
        mmu->memory_log,
        "At time %d disk address %d for process %d is loaded into memory page %d.\n",
        now, disk_address, pcb->id, frame
    );
    fflush(mmu->memory_log);

    return frame;
}

int mmu_load_page(MMU *mmu, PCB *pcb, int virtual_page, char op, int now, int *disk_ticks)
{
    int frame = mmu_reserve_page_frame(mmu, pcb, virtual_page, op, disk_ticks);
    if (frame == -1)
        return -1;

    return mmu_complete_page_load(mmu, pcb, virtual_page, op, frame, now);
}

int mmu_load_initial_page(MMU *mmu, PCB *pcb, int now)
{
    if (pcb->limit <= 0)
        return 0;

    int disk_ticks = 0;
    int frame = mmu_prepare_frame_for_data_page(mmu, &disk_ticks, 1);
    if (frame == -1)
        return -1;

    if (mmu_map_data_page(mmu, pcb, 0, 'r', frame) == -1)
        return -1;

    int disk_address = pcb->base;

    fprintf(
        mmu->memory_log,
        "At time %d disk address %d for process %d is loaded into memory page %d.\n",
        now, disk_address, pcb->id, frame
    );
    fflush(mmu->memory_log);

    return frame;
}
int mmu_access_memory(MMU *mmu, PCB *pcb, int virtual_address, char op, int now)
{
    int virtual_page = virtual_address / PAGE_SIZE_BYTES;

    if (virtual_page < 0 || virtual_page >= pcb->limit)
        return -1;

     if (!pcb->page_table[virtual_page].present)
    {
        fprintf(
            mmu->memory_log,
            "PageFault upon VA 0x%x from process %d\n",
            virtual_address,
            pcb->id
        );
        fflush(mmu->memory_log);
        return 0;
    }

    int frame = pcb->page_table[virtual_page].frame_number;

    pcb->page_table[virtual_page].referenced = 1;
    mmu->frames[frame].referenced = 1;

    if (op == 'w')
    {
        pcb->page_table[virtual_page].modified = 1;
        mmu->frames[frame].modified = 1;
    }

    return 1;
}
void mmu_free_process(MMU *mmu, PCB *pcb)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (mmu->frames[i].owner_pid == pcb->id)
        {
            mmu_free_frame(mmu, i);
        }
    }

    if (pcb->page_table != NULL)
    {
        free(pcb->page_table);
        pcb->page_table = NULL;
    }
}
