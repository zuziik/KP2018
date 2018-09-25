/* See COPYRIGHT for copyright information. */

#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>

#include <kern/cpu.h>
#include <kern/env.h>
#include <kern/pmap.h>
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
    if (va < 0) {
        return (void *) -1;
    }

    // Insert the new vma
    new_vma = vma_insert(curenv, VMA_ANON, (void *) va, size_r, perm, NULL, NULL, 0);
    if (new_vma == NULL) {
        return (void *) -1;
    }

    // MAP_POPULATE: Map the whole vma directly into page tables
    if (flags) {
        vma_map_populate((uintptr_t) new_vma->va, new_vma->len, perm, curenv);
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
            new_vma = vma_insert(curenv, vma->type, (void *)va_end, len_new, vma->perm, NULL, NULL, 0);

        }
    }

    // Unmap all pages
    vma_unmap(va_start, size_rounded, curenv);
    return 0;
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
        // MATTHIJS: cast must be here else make error
        case SYS_vma_create: return (uintptr_t) sys_vma_create((size_t) a1, (int) a2, (int) a3);
        case SYS_vma_destroy: return sys_vma_destroy((void *) a1, (size_t) a2);
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
