/* See COPYRIGHT for copyright information. */

#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>

#include <kern/cpu.h>
#include <kern/env.h>
#include <kern/idt.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>
#include <kern/spinlock.h>
#include <kern/syscall.h>

#include <kern/vma.h>
#include <kern/lock.h>

struct env *envs = NULL;            /* All environments */
static struct env *env_free_list;   /* Free environment list */            
                                    /* (linked by env->env_link) */

#define ENVGENSHIFT 12      /* >= LOGNENV */

/*
 * Converts an envid to an env pointer.
 * If checkperm is set, the specified environment must be either the
 * current environment or an immediate child of the current environment.
 *
 * RETURNS
 *   0 on success, -E_BAD_ENV on error.
 *   On success, sets *env_store to the environment.
 *   On error, sets *env_store to NULL.
 */
int envid2env(envid_t envid, struct env **env_store, bool checkperm)
{
    struct env *e;

    int kern = env_lock_env();
    assert_lock_env();

    /* If envid is zero, return the current environment. */
    if (envid == 0) {
        *env_store = curenv;
        env_unlock_env(kern);
        return 0;
    }

    /*
     * Look up the env structure via the index part of the envid,
     * then check the env_id field in that struct env
     * to ensure that the envid is not stale
     * (i.e., does not refer to a _previous_ environment
     * that used the same slot in the envs[] array).
     */
    e = &envs[ENVX(envid)];
    if (e->env_status == ENV_FREE || e->env_id != envid) {
        *env_store = 0;
        env_unlock_env(kern);
        return -E_BAD_ENV;
    }

    /*
     * Check that the calling environment has legitimate permission
     * to manipulate the specified environment.
     * If checkperm is set, the specified environment
     * must be either the current environment
     * or an immediate child of the current environment.
     */
    if (checkperm && e != curenv && e->env_parent_id != curenv->env_id) {
        *env_store = 0;
        env_unlock_env(kern);
        return -E_BAD_ENV;
    }

    *env_store = e;
    env_unlock_env(kern);
    return 0;
}

/*
 * Mark all environments in 'envs' as free, set their env_ids to 0,
 * and insert them into the env_free_list.
 * Make sure the environments are in the free list in the same order
 * they are in the envs array (i.e., so that the first call to
 * env_alloc() returns envs[0]).
 */
void env_init(void)
{
    /* Set up envs array. */
    /* LAB 3: your code here. */
    cprintf("[ENV INIT] start\n");

    env_free_list = NULL;
    struct env *e;
    int i;

    for (i = NENV - 1; i >= 0; i--) {
        e = &envs[i];
        e->env_id = 0;
        e->env_status = ENV_FREE;
        e->env_link = env_free_list;
        env_free_list = e;
    }
    
    cprintf("[ENV INIT] end\n");
}

/*
 * Initialize the kernel virtual memory layout for environment e.
 * Allocate a page directory, set e->env_pml4 accordingly,
 * and initialize the kernel portion of the new environment's address space.
 * Do NOT (yet) map anything into the user portion
 * of the environment's virtual address space.
 *
 * Returns 0 on success, < 0 on error.  Errors include:
 *  -E_NO_MEM if page directory or table could not be allocated.
 */
static int env_setup_vm(struct env *e)
{
    cprintf("[ENV SETUP VM] start\n");

    int i;
    struct page_info *p = NULL;

    /* Allocate a page for the page directory */
    if (!(p = page_alloc(ALLOC_ZERO))) {
        return -E_NO_MEM;
    }

    /*
     * Now, set e->env_pml4 and initialize the page directory.
     *
     * Hint:
     *    - The VA space of all envs is identical above UTOP
     *  (except at UVPT, which we've set below).
     *  See inc/memlayout.h for permissions and layout.
     *  Can you use kern_pgdir as a template?  Hint: Yes.
     *  (Make sure you got the permissions right in Lab 2.)
     *    - The initial VA below UTOP is empty.
     *    - You do not need to make any more calls to page_alloc.
     *    - Note: In general, pp_ref is not maintained for physical pages mapped
     *      only above UTOP, but env_pml4 is an exception -- you need to
     *      increment env_pml4's pp_ref for env_free to work correctly.
     *    - The functions in kern/pmap.h are handy.
     */

    /* LAB 3: your code here. */
    page_increm(p);
    e->env_pml4 = (struct page_table *)KADDR(page2pa(p));

    // The initial VA below UTOP is empty
    for (i = 0; i < PML4_INDEX(USER_TOP); i++) {
        e->env_pml4->entries[i] = 0;
    }

    // The VA space of all envs is identical above UTOP
    // (we don't allocate any new pages, we use the existing structures)
    for (i = PML4_INDEX(USER_TOP); i < PAGE_TABLE_ENTRIES; i++) {
        e->env_pml4->entries[i] = kern_pml4->entries[i];
    }

    /* UVPT maps the env's own page table read-only.
     * Permissions: kernel R, user R */
    e->env_pml4->entries[PML4_INDEX(USER_PML4)] =
        PADDR(e->env_pml4) | PAGE_PRESENT | PAGE_USER | PAGE_NO_EXEC;

    cprintf("[ENV SETUP VM] end\n");
    return 0;
}

// Init all the values in the vma structure
static int env_setup_vma(struct env *e) {
    cprintf("[ENV SETUP VMA] start\n");
    struct vma *vma_list = e->vma_array;
    int j;

    for (j = 0; j < MAX_VMAS; j++) {
        vma_list[j].type = VMA_UNUSED;       
        vma_list[j].va = NULL;
        vma_list[j].len = 0;  
        vma_list[j].perm = 0;    
        vma_list[j].mem_va = NULL;
        vma_list[j].mem_size = 0;
        vma_list[j].file_va = NULL;
        vma_list[j].file_size = 0;
        vma_list[j].next = (j == MAX_VMAS-1) ? NULL : &vma_list[j+1];
        vma_list[j].prev = (j == 0) ? NULL : &vma_list[j-1];
    }

    e->vma = vma_list;
    cprintf("[ENV SETUP VMA] end\n");
    return 0;
}

/*
 * Allocates and initializes a new environment.
 * On success, the new environment is stored in *newenv_store.
 *
 * Returns 0 on success, < 0 on failure.  Errors include:
 *  -E_NO_FREE_ENV if all NENVS environments are allocated
 *  -E_NO_MEM on memory exhaustion
 */
int env_alloc(struct env **newenv_store, envid_t parent_id)
{
    cprintf("[ENV ALLOC] start\n");
    int kern = env_lock_env();
    int32_t generation;
    int r;
    struct env *e;

    if (!(e = env_free_list)) {
        env_unlock_env(kern);
        return -E_NO_FREE_ENV;
    }

    /* Allocate and set up the page directory for this environment. */
    if ((r = env_setup_vm(e)) < 0) {
        env_unlock_env(kern);
        return r;
    }

    /* Set up list of VMAs for this environment. */
    if ((r = env_setup_vma(e)) < 0) {
        env_unlock_env(kern);
        return r;
    }

    /* Generate an env_id for this environment. */
    generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
    if (generation <= 0)    /* Don't create a negative env_id. */
        generation = 1 << ENVGENSHIFT;
    e->env_id = generation | (e - envs);

    /* Set the basic status variables. */
    e->env_parent_id = parent_id;
    e->env_type = ENV_TYPE_USER;
    e->env_status = ENV_RUNNABLE;
    e->env_runs = 0;

    e->timeslice = MAXTIMESLICE;
    e->prev_time = 0;
    e->pause = -1;

    /*
     * Clear out all the saved register state, to prevent the register values of
     * a prior environment inhabiting this env structure from "leaking" into our
     * new environment.
     */
    memset(&e->env_frame, 0, sizeof e->env_frame);

    /*
     * Set up appropriate initial values for the segment registers.
     * GD_UD is the user data segment selector in the GDT, and
     * GD_UT is the user text segment selector (see inc/memlayout.h).
     * The low 2 bits of each segment register contains the
     * Requestor Privilege Level (RPL); 3 means user mode.  When
     * we switch privilege levels, the hardware does various
     * checks involving the RPL and the Descriptor Privilege Level
     * (DPL) stored in the descriptors themselves.
     */
    e->env_frame.ds = GDT_UDATA | 3;
    e->env_frame.ss = GDT_UDATA | 3;
    e->env_frame.rsp = USTACK_TOP;
    e->env_frame.cs = GDT_UCODE | 3;
    /* You will set e->env_frame.rip later. */

    /* Enable interrupts while in user mode.
     * LAB 5: your code here. */
    // FLAGS_IF: if 0, interrupts are disabled, if 1, interrupts are enabled
    if (!(read_rflags() & FLAGS_IF)) {
        e->env_frame.rflags |= FLAGS_IF;
    }

    /* Commit the allocation */
    env_free_list = e->env_link;
    *newenv_store = e;

    cprintf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
    env_unlock_env(kern);
    cprintf("[ENV ALLOC] end\n");
    return 0;
}

/*
 * Allocate len bytes of physical memory for environment env, and map it at
 * virtual address va in the environment's address space.
 * Does not zero or otherwise initialize the mapped pages in any way.
 * Pages should be writable by user and kernel.
 * Panic if any allocation attempt fails.
 */
// static void region_alloc(struct env *e, void *va, size_t len)
// {
//     /*
//      * LAB 3: Your code here.
//      * (But only if you need it for load_icode.)
//      *
//      * Hint: It is easier to use region_alloc if the caller can pass
//      *   'va' and 'len' values that are not page-aligned.
//      *   You should round va down, and round (va + len) up.
//      *   (Watch out for corner-cases!)
//      */

//     cprintf("[REGION ALLOC] start\n");

//     struct page_info *p = NULL;
//     uintptr_t va_p = (uintptr_t) va;
//     uintptr_t va_start = ROUNDDOWN(va_p, PAGE_SIZE);
//     uintptr_t va_end = ROUNDUP(va_p + len, PAGE_SIZE);
//     uintptr_t vi;

//     for (vi = va_start; vi < va_end; vi += PAGE_SIZE) {
//         if (!(p = page_alloc(ALLOC_ZERO)))
//             panic("Couldn't allocate memory for environment");
//         page_insert(e->env_pml4, p, (void *)vi, PAGE_WRITE | PAGE_USER);
//     }

//     cprintf("[REGION ALLOC] end\n");
// }

/*
 * Set up the initial program binary, stack, and processor flags for a user
 * process.
 * This function is ONLY called during kernel initialization, before running the
 * first user-mode environment.
 *
 * This function loads all loadable segments from the ELF binary image into the
 * environment's user memory, starting at the appropriate virtual addresses
 * indicated in the ELF program header.
 * At the same time it clears to zero any portions of these segments that are
 * marked in the program header as being mapped but not actually present in the
 * ELF file - i.e., the program's bss section.
 *
 * All this is very similar to what our boot loader does, except the boot loader
 * also needs to read the code from disk. Take a look at boot/main.c to get
 * ideas.
 *
 * Finally, this function maps one page for the program's initial stack.
 *
 * load_icode panics if it encounters problems.
 *  - How might load_icode fail?  What might be wrong with the given input?
 */
static void load_icode(struct env *e, uint8_t *binary)
{
    /*
     * Hints:
     *  Load each program segment into virtual memory at the address specified
     *  in the ELF section header.
     *  You should only load segments with ph->p_type == ELF_PROG_LOAD.
     *  Each segment's virtual address can be found in ph->p_va and its size in
     *  memory can be found in ph->p_memsz.
     *  The ph->p_filesz bytes from the ELF binary, starting at 'binary +
     *  ph->p_offset', should be copied to virtual address ph->p_va.
     *  Any remaining memory bytes should be cleared to zero.
     *  (The ELF header should have ph->p_filesz <= ph->p_memsz.)
     *  Use functions from the previous lab to allocate and map pages.
     *
     *  All page protection bits should be user read/write for now.
     *  ELF segments are not necessarily page-aligned, but you can assume for
     *  this function that no two segments will touch the same virtual page.
     *
     *  You may find a function like region_alloc useful.
     *
     *  Loading the segments is much simpler if you can move data directly into
     *  the virtual addresses stored in the ELF binary.
     *  So which page directory should be in force during this function?
     *
     *  You must also do something with the program's entry point, to make sure
     *  that the environment starts executing there.
     *  What?  (See env_run() and env_pop_frame() below.)
     */

    /* LAB 3: your code here. */

    /* load each program segment (ignores ph flags) */

    cprintf("[LOAD ICODE] start\n");

    struct elf *eh = (struct elf *)binary;
    struct elf_proghdr *ph;
    int i, number_of_segments;

    if (eh->e_magic != ELF_MAGIC)
        panic("Invalid ELF magic");

    ph = (struct elf_proghdr *) ((uint8_t *) eh + eh->e_phoff);
    number_of_segments = eh->e_phnum;

    for (i = 0; i < number_of_segments; i++) {
        // Create a vma mapping for each program segment
        // Load binaries
        if (ph[i].p_type == ELF_PROG_LOAD) {
            if (vma_insert(e, VMA_BINARY, (void *)ph[i].p_va, 
                ph[i].p_memsz, PAGE_WRITE | PAGE_USER, 
                binary + ph[i].p_offset, ph[i].p_filesz) == NULL) {
                panic("Couldn't create VMA for a program segment");
            }
            cprintf("ELF segment: kernel %llx, user %llx, size %llx", 
                binary + ph[i].p_offset, ph[i].p_va, ph[i].p_filesz);
        }
    }
    
    e->env_frame.rip = eh->e_entry;

    // Create a Vma for the user stack
    vma_insert(e, VMA_ANON, (void *)(USTACK_TOP - PAGE_SIZE), PAGE_SIZE, 
               PAGE_WRITE | PAGE_USER, NULL, 0);

    /* vmatest binary uses the following */
    /* 1. Map one RO page of VMA for UTEMP at virtual address UTEMP.
     * 2. Map one RW page of VMA for UTEMP+PAGE_SIZE at virtual address UTEMP. */

    /* LAB 4: Your code here. */
    vma_insert(e, VMA_ANON, UTEMP, PAGE_SIZE, PAGE_USER,
            NULL, 0);
    vma_insert(e, VMA_ANON, UTEMP+PAGE_SIZE, PAGE_SIZE, PAGE_WRITE | PAGE_USER,
            NULL, 0);

    cprintf("[LOAD ICODE] end\n");
}

/*
 * Allocates a new env with env_alloc, loads the named elf binary into it with
 * load_icode, and sets its env_type.
 * This function is ONLY called during kernel initialization, before running the
 * first user-mode environment.
 * The new env's parent ID is set to 0.
 */
void env_create(uint8_t *binary, enum env_type type)
{
    /* LAB 3: your code here. */
    cprintf("[ENV CREATE] start\n");
    int kern = env_lock_env();
    struct env *e;
    int res;

    res = env_alloc(&e, 0);
    // Successs
    if (res == 0) {
        load_icode(e, binary);
        e->env_type = type;
    }
    else if (res == E_NO_MEM) {
        panic("Failed to allocate memory for the environment");
    }
    else if (res == E_NO_FREE_ENV) {
        panic("No free environment");
    }
    else {
        panic("Error in env_alloc");
    }

    env_unlock_env(kern);
    cprintf("[ENV CREATE] end\n");
}

void env_free_page_tables(struct page_table *page_table, size_t depth)
{
    struct page_table *child;
    physaddr_t *entry;
    size_t i, max;

    // Commented line was in default framework but wrong, next line is correct
    // max = (depth == 3) ? PML4_INDEX(KERNEL_VMA) : PAGE_TABLE_ENTRIES;
    max = (depth == 3) ? PML4_INDEX(USER_TOP) : PAGE_TABLE_ENTRIES;

    /* Iterate the entries in the page table to free them. */
    for (i = 0; i < max; ++i) {
        entry = page_table->entries + i;

        /* Skip unused entries. */
        if (!(*entry & PAGE_PRESENT))
            continue;

        if (depth) {
            /* Free the page table. */
            child = KADDR(PAGE_ADDR(*entry));
            env_free_page_tables(child, depth - 1);
        } else {
            /* Free the page. */
            page_decref(pa2page(PAGE_ADDR(*entry)));
            *entry = 0;
        }
    }

    /* Free the page table. */
    page_decref(pa2page(PADDR(page_table)));
}

/*
 * Frees env e and all memory it uses.
 */
void env_free(struct env *e)
{
    int kern = env_lock_env();
    /* If freeing the current environment, switch to kern_pgdir
     * before freeing the page directory, just in case the page
     * gets reused. */
    if (e == curenv)
        load_pml4((struct page_table *)PADDR(kern_pml4));

    /* Note the environment's demise. */
    cprintf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

    /* Free the page tables. */
    static_assert(USER_TOP % PAGE_SIZE == 0);

    env_free_page_tables(e->env_pml4, 3);
    e->env_pml4 = NULL;

    /* Return the environment to the free list */
    e->env_status = ENV_FREE;
    e->env_link = env_free_list;
    env_free_list = e;

    env_unlock_env(kern);
}

/*
 * Frees environment e.
 * If e was the current env, then runs a new environment (and does not return
 * to the caller).
 */
void env_destroy(struct env *e)
{
    int kern = env_lock_env();
    assert_lock_env();

    /* If e is currently running on other CPUs, we change its state to
     * ENV_DYING. A zombie environment will be freed the next time
     * it traps to the kernel. */
    if (e->env_status == ENV_RUNNING && curenv != e) {
        e->env_status = ENV_DYING;
        return;
    }

    env_free(e);

    if (curenv == e) {
        // Curenv just finished so reset the envs which were paused
        reset_pause(get_env_index(curenv->env_id));

        sched_yield();
    }

    env_unlock_env(kern);
}

/*
 * Restores the register values in the trapframe with the 'iret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */
void env_pop_frame(struct int_frame *frame)
{
    /* Record the CPU we are running on for user-space debugging */
    curenv->env_cpunum = cpunum();

    switch (frame->int_no) {
#ifdef LAB3_SYSCALL
    case 0x80: sysret64(frame); break;
#endif
    default: iret64(frame); break;
    }

    panic("iret failed");  /* mostly to placate the compiler */
}

/*
 * Context switch from curenv to env e.
 * Note: if this is the first call to env_run, curenv is NULL.
 *
 * This function does not return.
 */
void env_run(struct env *e)
{
    /*
     * Step 1: If this is a context switch (a new environment is running):
     *     1. Set the current environment (if any) back to
     *        ENV_RUNNABLE if it is ENV_RUNNING (think about
     *        what other states it can be in),
     *     2. Set 'curenv' to the new environment,
     *     3. Set its status to ENV_RUNNING,
     *     4. Update its 'env_runs' counter,
     *     5. Use lcr3() to switch to its address space.
     * Step 2: Use env_pop_tf() to restore the environment's
     *     registers and drop into user mode in the
     *     environment.
     *
     * Hint: This function loads the new environment's state from
     *  e->env_tf.  Go back through the code you wrote above
     *  and make sure you have set the relevant parts of
     *  e->env_tf to sensible values.
     */

    /* LAB 3: your code here. */
    cprintf("[ENV_RUN] start\n");

    int lock = env_lock_env();

    // If there is any already running environment, make it runnable
    if (curenv != NULL && curenv->env_status == ENV_RUNNING && curenv->env_cpunum == cpunum())
        curenv->env_status = ENV_RUNNABLE;

    // switch to new environment
    curenv = e;
    curenv->env_status = ENV_RUNNING;
    curenv->env_runs += 1;

    // First run env, is not initialized correct yet
    if (curenv->prev_time == 0) {
        curenv->prev_time = read_tsc();
    }

    cprintf("%d[ENV_RUN] curenv_id: %d\n", cpunum(), curenv->env_id);
    unlock_env();

    load_pml4((void *)PADDR(curenv->env_pml4));
    env_pop_frame(&curenv->env_frame);
}
