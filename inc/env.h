/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ENV_H
#define JOS_INC_ENV_H

#include <inc/types.h>

#include <inc/x86-64/idt.h>
#include <inc/x86-64/memory.h>

typedef int32_t envid_t;

/*
 * An environment ID 'envid_t' has three parts:
 *
 * +1+---------------21-----------------+--------10--------+
 * |0|          Uniqueifier             |   Environment    |
 * | |                                  |      Index       |
 * +------------------------------------+------------------+
 *                                       \--- ENVX(eid) --/
 *
 * The environment index ENVX(eid) equals the environment's offset in the
 * 'envs[]' array.  The uniqueifier distinguishes environments that were
 * created at different times, but share the same environment index.
 *
 * All real environments are greater than 0 (so the sign bit is zero).
 * envid_ts less than 0 signify errors.  The envid_t == 0 is special, and
 * stands for the current environment.
 */

#define LOG2NENV        7
#define NENV            (1 << LOG2NENV)
#define ENVX(envid)     ((envid) & (NENV - 1))
#define MAXTIMESLICE    100000000
#define MAX_KTHREADS    32
#define MAX_VMAS        128
#define MAX_WAITTIME    500000000

/* Values of env_status in struct env */
enum {
    ENV_FREE = 0,
    ENV_DYING,
    ENV_RUNNABLE,
    ENV_RUNNING,
    ENV_NOT_RUNNABLE
};

/* Special environment types */
enum env_type {
    ENV_TYPE_USER = 0,
};

struct env {
    struct int_frame env_frame; /* Saved registers */
    struct env *env_link;       /* Next free env */
    envid_t env_id;             /* Unique environment identifier */
    envid_t env_parent_id;      /* env_id of this env's parent */
    enum env_type env_type;     /* Indicates special system environments */
    unsigned env_status;        /* Status of the environment */
    uint32_t env_runs;          /* Number of times environment has run */
    int env_cpunum;             /* The CPU that the env is running on */

    /* Address space */
    struct page_table *env_pml4;

    // Linked list of vma's and current amount of vma's (128 max)
    struct vma *vma;

    // Array of allocated VMAs (this is in order on disk, unlike the previous one)
    // --> this pointer always points at the first VMA and we can access them all
    // as an array (used when allocating a new environment)
    struct vma *vma_array;

    // Keep track of the timeslice                     
    int64_t timeslice;
    int64_t prev_time;

    // For which env does it have to wait. -1 means no waiting
    envid_t pause;

    // LAB 7
    size_t num_alloc;        /* Number of used physical pages allocated by page_alloc in user space */
    size_t num_tables;       /* Number of pages used for page table tree */
    size_t num_swap;         /* Number of swapped out pages */
};

/* Anonymous VMAs are zero-initialized whereas binary VMAs
 * are filled-in from the ELF binary.
 */
enum {
    VMA_UNUSED,
    VMA_ANON,
    VMA_BINARY,
};

struct vma {
    int type;           // See enum above
    void *va;           // Start virt addr (alligned)
    size_t len;         // Length of virt addr block (alligned)
    int perm;           // Permissions

    /* LAB 4: You may add more fields here, if required. */
    struct vma *next;
    struct vma *prev;

    void* mem_va;       // Not alligned virt addr user space, binary dest
    void* file_va;      // Not alligned virt addr kernel space, binary source
    uint64_t mem_size;  // "..." length "... ", binary dest length
    uint64_t file_size; // "..." length "... ", binary source length

    /* LAB 7 currently swapped out VAs */
    struct swapped_va *swapped_pages;
};

struct kthread {
    envid_t kt_id;                  /* Unique kernel thread identifier */
    unsigned kt_status;             /* Status of the kernel thread */
    int kt_cpunum;                  /* The CPU that the env is running on */
    uint64_t start_rip;             /* address of the kernel thread routine start */
    uint64_t start_rbp;             /* TOP of the stack for the kernel thread */
    void *top;                      /* also top of the stack but of another type */
    uint64_t rsp;                   /* Current stack pointer for the kernel thread */

    // Keep track of the timeslice                     
    int64_t timeslice;
    int64_t prev_time;
};

#endif /* !JOS_INC_ENV_H */
