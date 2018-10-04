#include <inc/x86-64/gdt.h>
#include <inc/x86-64/memory.h>

#include <kern/cpu.h>
#include <kern/gdt.h>

struct gdt_entry gdt_entries[5 + NCPU * 2] = {
    [GDT_KCODE >> 3] = { .flags = GDT_KCODE_FLAGS | GDT_LONG_MODE },
    [GDT_KDATA >> 3] = { .flags = GDT_KDATA_FLAGS },
    [GDT_UCODE >> 3] = { .flags = GDT_UCODE_FLAGS | GDT_LONG_MODE },
    [GDT_UDATA >> 3] = { .flags = GDT_UDATA_FLAGS },
};

struct gdtr gdtr = {
    .limit = sizeof(gdt_entries) - 1,
    .entries = gdt_entries,
};

void gdt_init(void)
{
    gdt_init_percpu();
}

void gdt_init_percpu(void)
{
    size_t i = cpunum();

    /* Set up the kernel stack pointer in the TSS. Add the TSS to the GDT. Load
     * the GDT and the task selector.
     */

    /*
     * Update the code below to set up the kernel stack for each CPU.
     *
     * LAB 6: your code here.
     */
    // Matthijs: see pmap.c, mem_init_mp?
    // This code was already here during lab 3 at least
    thiscpu->cpu_tss.rsp[0] = KSTACK_TOP;

    /*
     * The code below has been updated to set up a per-CPU TSS, load the GDT
     * and load the right task selector.
     */
    set_tss_entry((struct tss_entry *)(gdt_entries + (GDT_TSS0 >> 3) + i * 2),
        &thiscpu->cpu_tss);
    load_gdt(&gdtr, GDT_KCODE, GDT_KDATA);
    load_task_sel(GDT_TSS0 + ((i * 2) << 3));
}
