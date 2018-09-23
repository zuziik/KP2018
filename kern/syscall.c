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
// MATTHIJS: TODO -> FLAGS
static void *sys_vma_create(size_t size, int perm, int flags)
{
    /* Virtual Memory Area allocation */
    /* LAB 4: Your code here. */
    // MATTHIJS: Is this the correct way to search for free mem
    uintptr_t va_start = vma_get_vmem(size, curenv->vma);
    struct vma *new_vma, *tmp;

    // Could not get virtual mem for new vma
    if (va_start == -1) {
        return (void *) -1;
    }

    // Set new vma
    new_vma->type = VMA_ANON;
    new_vma->va = (void *) va_start;
    new_vma->len = size;                // MATTHIJS: Allign size?
    new_vma->perm = perm;
    curenv->vma_num++;

    // Insert the new vma
    if (!vma_insert(new_vma, curenv)) {
        return (void *) -1;
    }

    // MAP_POPULATE: Map the whole vma direct into page tables
    if (flags) {
        vma_map_populate((uintptr_t) new_vma->va, new_vma->len, perm);
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
    struct vma *vma = vma_lookup(curenv, va);
    if (vma == NULL) {
        panic("Can not destroy a non-existing VMA\n");
    }

    // Can only destory 1 vma
    if ((uintptr_t) va + size > (uintptr_t) vma->va + vma->len) {
        panic("Trying to destroy more than 1 VMA\n");
        return -1;
    }
    // Destroy whole vma
    else if ((uintptr_t) va == (uintptr_t) vma->va &&
             size == vma->len) {
        // Remove from vma list
        // First in list
        if (vma->prev == NULL) {
            curenv->vma = vma->next;
            if (curenv->vma != NULL) {
                (vma->next)->prev = NULL;
            }
        } 
        // Not first entry
        else {
            (vma->prev)->next = vma->next;
            if (vma->next != NULL) {
                (vma->next)->prev = vma->prev;
            }
        }

        curenv->vma_num--;

        // TODO: clear all entries and tables if needed
    } 
    // Destroy part of vma
    else {
        // MATTHIJS: TODO
    }

   return -1;
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
        // MATTHIJS: arguments?
        // case SYS_vma_create: return sys_vma_create();
        // case sys_vma_destroy: return sys_vma_destroy();
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
