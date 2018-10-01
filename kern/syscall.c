/* See COPYRIGHT for copyright information. */

#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>

#include <kern/cpu.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/console.h>

#include <kern/vma.h>

extern void syscall64(void);

void syscall_init(void)
{
    syscall_init_percpu();
}

void syscall_init_percpu(void)
{
    /* LAB 3: your code here. */
}

/*
 * Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors.
 */
static void sys_cputs(const char *s, size_t len)
{
    /* Check that the user has permission to read memory [s, s+len).
     * Destroy the environment if not. */
    
    /* LAB 3: your code here. */ 
    user_mem_assert(curenv, (void *)s, len, 0);

    // Only executes this code if everything is correct
    /* Print the string supplied by the user. */
    cprintf("%.*s", len, s);
}

/*
 * Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting.
 */
static int sys_cgetc(void)
{
    return cons_getc();
}

/* Returns the current environment's envid. */
static envid_t sys_getenvid(void)
{
    return curenv->env_id;
}

/*
 * Destroy a given environment (possibly the currently running environment).
 *
 * Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 */
static int sys_env_destroy(envid_t envid)
{
    int r;
    struct env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
        return r;

    if (e == curenv)
        cprintf("[%08x] exiting gracefully\n", curenv->env_id);
    else
        cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);

    env_destroy(e);

    return 0;
}

/*
 * Creates a new anonymous mapping somewhere in the virtual address space.
 *
 * Supported flags: 
 *     MAP_POPULATE
 * 
 * Returns the address to the start of the new mapping, on success,
 * or -1 if request could not be satisfied.
 */
static void *sys_vma_create(size_t size, int perm, int flags)
{
    /* Virtual Memory Area allocation */
    /* LAB 4: Your code here. */
    uintptr_t va;
    struct vma *new_vma;

    // Round up the size
    size_t size_r = ROUNDUP(size, PAGE_SIZE);

    // Find available chunk of virtual memory
    va = vma_get_vmem(size_r, curenv->vma);
    if ((long long) va < 0) {
        return (void *) -1;
    }

    // Insert the new vma
    new_vma = vma_insert(curenv, VMA_ANON, (void *) va, size, perm | PAGE_USER, NULL, 0);
    if (new_vma == NULL) {
        return (void *) -1;
    }

    // MAP_POPULATE: Map the whole vma directly into page tables
    if (flags) {
        vma_map_populate((uintptr_t) new_vma->va, new_vma->len, perm | PAGE_USER, curenv);
    }

    return new_vma->va;
}

/*
 * Unmaps the specified range of memory starting at 
 * virtual address 'va', 'size' bytes long.
 */
static int sys_vma_destroy(void *va, size_t size)
{
    /* Virtual Memory Area deallocation */
    /* LAB 4: Your code here. */
 
    // Round the addresses
    uintptr_t va_start = ROUNDUP((uintptr_t) va, PAGE_SIZE);
    uintptr_t va_end = ROUNDDOWN((uintptr_t) va + size, PAGE_SIZE);
    size_t size_rounded = (size_t) va_end - va_start;

    struct vma *new_vma;
    struct vma *vma = vma_lookup(curenv, va);
    void *va_new;
    size_t len_new;

    if (vma == NULL) {
        panic("VA is not mapped anywhere, cannot unmap\n");
        return -1;
    }
    // Can not destory > 1 VMA at a time
    else if (va_end > (uintptr_t) vma->va + vma->len) {
        panic("Trying to unmap memory range spanning more than 1 VMA\n");
        return -1;
    }

    // Destroy 1 whole VMA
    if (va_start == (uintptr_t) vma->va &&
             size_rounded == vma->len) {
        vma_make_unused(curenv, vma);
    }

    // Destroy part of VMA
    else {
        // Destroy first part, keep second part
        if (va_start == (uintptr_t) vma->va) {
            vma->va = (void *) va_end;
            vma->len -= size_rounded;
        } 
        // Destroy last part, keep first part
        else if (va_end == (uintptr_t) vma->va + vma->len) {
            vma->len -= size_rounded;
        }
        // Destory a part in the middle
        else {
            // Create new vma at end and insert it correctly
            len_new = ((uintptr_t)vma->va + vma->len) - va_end;

            // Fix length of first segment of the VMA
            vma->len = vma->len - size_rounded - len_new;

            // Now add the last VMA segment as a new VMA
            new_vma = vma_insert(curenv, vma->type, (void *)va_end, len_new, vma->perm, NULL, 0);

        }
    }

    // Unmap all pages
    vma_unmap(va_start, size_rounded, curenv);
    return 0;
}

/*
 * Deschedule current environment and pick a different one to run.
 */
static void sys_yield(void)
{
    sched_yield();
}

/* MATTHIJS
 * Pause curenv and run the env with envid
 * Continue executing curenv only if env is finished
 */
static int sys_wait(envid_t envid)
{
    /* LAB 5: your code here. */
    // Check if envid is a valid id
    if (get_env_index(envid) < 0)
        return -1;
    
    // Let curenv wait and reschedule immediately
    curenv->pause = envid;
    sched_yield();
    panic("[SYS_WAIT] sched_yield does return (remove this panic eventually)\n");
    return 0;
}

// Remove all PAGE_WRITE permissions from the pml4
void enforce_cow(struct page_table *pml4) {
    cprintf("[ENFORCE_COW] Start\n");
    struct page_table *pdpt, *pgdir, *pt;
    size_t s, t, u, v;

    // Loop through pml4 entries
    for (s = 0; s < PAGE_TABLE_ENTRIES; ++s) {
        if (!(pml4->entries[s] & PAGE_PRESENT))
            continue;

        // Loop through pdp entries
        pdpt = (void *)(KERNEL_VMA + PAGE_ADDR(pml4->entries[s]));
        for (t = 0; t < PAGE_TABLE_ENTRIES; ++t) {
            if (!(pdpt->entries[t] & PAGE_PRESENT))
                continue;

            // Loop through pd entries
            pgdir = (void *)(KERNEL_VMA + PAGE_ADDR(pdpt->entries[t]));
            for (u = 0; u < PAGE_TABLE_ENTRIES; ++u) {
                if (!(pgdir->entries[u] & PAGE_PRESENT))
                    continue;

                // Check huge pages,
                if (pgdir->entries[u] & PAGE_HUGE && pgdir->entries[u] & PAGE_WRITE) {
                    // cprintf("[ENFORCE_COW] huge page\n");
                    pgdir->entries[u] &= ~PAGE_WRITE;
                    continue;
                }

                // Loop through page table
                pt = (void *)(KERNEL_VMA + PAGE_ADDR(pgdir->entries[u]));
                for (v = 0; v < PAGE_TABLE_ENTRIES; ++v) {
                    if (!(pt->entries[v] & PAGE_PRESENT))
                        continue;

                    if (pt->entries[v] & PAGE_WRITE) {
                        // cprintf("[ENFORCE_COW] normal page\n");
                        pt->entries[v] &= ~PAGE_WRITE;
                    }
                }
            }
        }
    }
}

// Copy the vma structure from old env to new env
void copy_vma(struct env *old, struct env *new) {
    int i;
    struct vma* vma_old = old->vma;
    struct vma* vma_new = new->vma;
    struct vma* vma_new_next;
    struct vma* vma_new_prev;

    for (i = 0; i < 128; i++) {
        vma_new->type = vma_old->type;
        vma_new->va = vma_old->va;
        vma_new->len = vma_old->len;
        vma_new->perm = vma_old->perm;

        vma_new->mem_va = vma_old->mem_va;
        vma_new->file_va = vma_old->file_va;
        vma_new->mem_size = vma_old->mem_size;
        vma_new->file_size = vma_old->file_size;

        // // Remember the original next and prev, set them after copy
        // vma_new_next = vma_new->next;
        // vma_new_prev = vma_new->prev;

        // memcpy((void *) &(vma_new), (void *) &(vma_old), sizeof(struct vma *));
        // vma_new->next = vma_new_next;
        // vma_new->prev = vma_new_prev;

        // Go to next vma
        vma_old = vma_old->next;
        vma_new = vma_new->next;
    }    
}

// Try to alloc a page for table in new page table based on old one
int alloc_table(struct page_table *old, struct page_table *new, size_t index) {
    struct page_info *p;
    int perm = PAGE_PRESENT;

    // Try to get a phsyical page for table entry
    if (!(p = page_alloc(ALLOC_ZERO))) {
        cprintf("[ALLOC_TABLE] out of memory\n");
        return -1;
    }

    // Get the permisisons based on old one
    if (old->entries[index] & PAGE_USER) {
        perm |= PAGE_USER;
    }
    if (old->entries[index] & PAGE_WRITE) {
        perm |= PAGE_WRITE;
    }
    if (old->entries[index] & PAGE_NO_EXEC) {
        perm |= PAGE_NO_EXEC;
    }
    if (old->entries[index] & PAGE_HUGE) {
        perm |= PAGE_HUGE;
    }

    // Set the page
    p->pp_ref++;
    new->entries[index] = PADDR((struct page_table *)KADDR(page2pa(p))) | perm;
    return 0;
}

// Copy all the page tables from old env to new env
// Only copy the physical mem associated with the tables, not the mappings
int copy_pml4(struct env *old, struct env *new) {
    cprintf("[COPY_PML4] Start\n");

    struct page_table *pdpt_new, *pdpt_old, *pgdir_new, *pgdir_old, *pt_new, *pt_old;
    size_t s, t, u, v;

    struct page_table *pml4_new = new->env_pml4;
    struct page_table *pml4_old = old->env_pml4;

    // Loop through pml4 entries
    for (s = 0; s < PAGE_TABLE_ENTRIES; ++s) {
        if (!(pml4_old->entries[s] & PAGE_PRESENT))
            continue;

        // We have pdp entry in pml4 - we must allocate one in new env, too
        if (alloc_table(pml4_old, pml4_new, s) < 0) {
            return -1;
        }
        
        // Loop through pdp entries
        pdpt_old = (void *)(KERNEL_VMA + PAGE_ADDR(pml4_old->entries[s]));
        pdpt_new = (void *)(KERNEL_VMA + PAGE_ADDR(pml4_new->entries[s]));
        for (t = 0; t < PAGE_TABLE_ENTRIES; ++t) {
            if (!(pdpt_old->entries[t] & PAGE_PRESENT))
                continue;

            if (alloc_table(pdpt_old, pdpt_new, t) < 0) {
                return -1;
            }

            // Loop through pd entries
            pgdir_old = (void *)(KERNEL_VMA + PAGE_ADDR(pdpt_old->entries[t]));
            pgdir_new = (void *)(KERNEL_VMA + PAGE_ADDR(pdpt_new->entries[t]));
            for (u = 0; u < PAGE_TABLE_ENTRIES; ++u) {
                if (!(pgdir_old->entries[u] & PAGE_PRESENT))
                    continue;

                // Check huge pages,
                if (pgdir_old->entries[u] & PAGE_HUGE) {
                    pgdir_new->entries[u] = pgdir_old->entries[u];
                    // memcpy((void *) &(pgdir_new->entries[u]), 
                    //        (void *) &(pgdir_old->entries[u]), sizeof(physaddr_t));
                    continue;
                }

                if (alloc_table(pgdir_old, pgdir_new, u) < 0) {
                    return -1;
                }

                // Loop through page table
                pt_old = (void *)(KERNEL_VMA + PAGE_ADDR(pgdir_old->entries[u]));
                pt_new = (void *)(KERNEL_VMA + PAGE_ADDR(pgdir_new->entries[u]));
                for (v = 0; v < PAGE_TABLE_ENTRIES; ++v) {
                    if (!(pt_old->entries[v] & PAGE_PRESENT))
                        continue;

                    pt_new->entries[v] = pt_old->entries[v];
                    // memcpy((void *) &(pt_new->entries[v]), 
                    //        (void *) &(pt_old->entries[v]), sizeof(physaddr_t));

                }
            }
        }
    }
    return 0;
}


void check_vma(struct vma *old_vma, struct vma *new_vma) {
    cprintf("[CHECK_VMA] Start\n");
    int i = 0;
    while (old_vma != NULL) {
        cprintf("i = %d\n", i);
        if (old_vma == new_vma) {
            cprintf("[CHECK_VMA] share same address! ERROR\n");
            break;
        }

        if (old_vma->va != new_vma->va) {
            cprintf("[CHECK_VMA] dont share same start address! ERROR\n");
            cprintf("[CHECK_VMA] old: %llx - new: %llx\n", old_vma->va, new_vma->va);
            break;
        }

        if (old_vma->len != new_vma->len) {
            cprintf("[CHECK_VMA] dont share same length! ERROR\n");
            break;
        }

        old_vma = old_vma->next;
        new_vma = new_vma->next;
        i++;
    }
    cprintf("[CHECK_VMA] done\n");
}




// Fork: create a new env based on the parent, curenv
// TODO:
// - are values in new_env correct?
// - memcpy or just new_env->... = curenv->...
// - how to do COW
// - what to return
static int sys_fork(void)
{
    /* fork() that follows COW semantics */
    /* LAB 5: your code here. */
    // return -1;
    cprintf("[SYS_FORK] START\n");
    struct env *new_env;
    if (env_free_list == NULL) {
        cprintf("[SYS_FORK] END CRASH\n");
        return -1;
    }

    // Get a new, free environment
    if (env_alloc(&new_env, curenv->env_id) < 0) {
        cprintf("[SYS_FORK] no free environments\n");
        return -1;
    }

    cprintf("[SYS_FORK] Before memcpy\n");

    // Copy data from VMA
    copy_vma(curenv, new_env);
    check_vma(curenv->vma, new_env->vma);

    cprintf("[SYS_FORK] cur vma: %llx\n", curenv->vma);
    cprintf("[SYS_FORK] new vma: %llx\n", new_env->vma);

    cprintf("[SYS_FORK] After vma copy\n");

    // Copy over the page tables
    copy_pml4(curenv, new_env);

    cprintf("[SYS_FORK] After pml4 copy\n");

    cprintf("[SYS_FORK] cur pml4: %llx\n", curenv->env_pml4);
    cprintf("[SYS_FORK] new pml4: %llx\n", new_env->env_pml4);

    // Enforce COW: remove all PAGE_WRITE permissions from leaves in pml4 of 
    // child and parent. VMA still has those permissions
    enforce_cow(curenv->env_pml4);
    enforce_cow(new_env->env_pml4);

    cprintf("[SYS_FORK] After enforce cow\n");

    // Copy the state
    memcpy((void *) &(new_env->env_frame), (void *) &(curenv->env_frame), 
           sizeof(curenv->env_frame));

    cprintf("[SYS_FORK] rflags old: %d - new: %d\n", 
            (curenv->env_frame).rflags, (new_env->env_frame).rflags);

    // Child return
    cprintf("[SYS_FORK] END\n");
    if (curenv->env_id == new_env->env_id) {
        return 0;
    }
    // Parent return
    else {
        return new_env->env_id;
    }
}

/* Dispatches to the correct kernel function, passing the arguments. */
int64_t syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3,
        uint64_t a4, uint64_t a5)
{
    /*
     * Call the function corresponding to the 'syscallno' parameter.
     * Return any appropriate return value.
     * LAB 3: Your code here.
     */

    switch (syscallno) {
        case SYS_cputs: sys_cputs((const char *)a1, (size_t) a2); return 0;
        case SYS_cgetc: return sys_cgetc();
        case SYS_getenvid: return sys_getenvid();
        case SYS_env_destroy: return sys_env_destroy((envid_t) a1);
        case SYS_vma_create: return (uintptr_t) sys_vma_create((size_t) a1, (int) a2, (int) a3);
        case SYS_vma_destroy: return sys_vma_destroy((void *) a1, (size_t) a2);
        case SYS_yield: sys_yield();
        case SYS_wait: return sys_wait((envid_t) a1);
        case SYS_fork: return sys_fork();
        default: return -E_NO_SYS;
    }
}

void syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx,
    uint64_t r8, uint64_t r9)
{
    struct int_frame *frame;

    /* Syscall from user mode. */
    assert(curenv);
    frame = &curenv->env_frame;

    /* Issue the syscall. */
    frame->rax = syscall(rdi, rsi, rdx, rcx, r8, r9);

    /* Return to the current environemnt, which should be running. */
    env_run(curenv);
}
