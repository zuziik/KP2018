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

#define LOG2NENV        10
#define NENV            (1 << LOG2NENV)
#define ENVX(envid)     ((envid) & (NENV - 1))

/* Values of env_status in struct env */
enum {
    ENV_FREE = 0,
    ENV_DYING,
    ENV_RUNNABLE,
    ENV_RUNNING,
    ENV_NOT_RUNNABLE
};

/* The interrupt method used to switch to the kernel. */
enum {
    ENV_INT = 0,
    ENV_SYSCALL,
};

/* Special environment types */
enum env_type {
    ENV_TYPE_USER = 0,
};

/* Max VMAs supported per environment. */
#define NVMA 128

/*
 * Very simple VMA structure. We support two types of VMA's: anonymous (e.g.,
 * heap/stack) that return zeroed pages. The other is sort of equivalant of the
 * memory-mapped files on for instance Linux, where a file/binary is loaded on
 * demand. As we don't have files or a file system, this can only happen in vOS
 * for the program binary.
 */
struct vma {
	struct vma *link;
    int type;
    void *va;
    size_t len;
    char *name;
    int perm;
    void *src_va; /* Location in kernel addr space of binary region. */
    size_t src_len;
};

enum {
    VMA_UNUSED,
    VMA_ANON,
    VMA_BINARY,
};

#define VMA_PERM_EXEC  1 /* Not used on IA-32. */
#define VMA_PERM_WRITE 2
#define VMA_PERM_READ  4 /* Always set. */

struct env {
    struct int_frame env_frame; /* Saved registers */
    struct env *env_wait;
    uint64_t env_tslice;
    struct env *env_link;       /* Next free env */
    envid_t env_id;             /* Unique environment identifier */
    envid_t env_parent_id;      /* env_id of this env's parent */
    enum env_type env_type;     /* Indicates special system environments */
    unsigned env_status;        /* Status of the environment */
    unsigned env_int;           /* Interrupt method. */
    uint32_t env_runs;          /* Number of times environment has run */
    int env_cpunum;             /* The CPU that the env is running on */

    /* Address space */
    struct page_table *env_pml4;
    struct vma *env_vmas;       /* Virtual memory areas of this env. */
};

#endif /* !JOS_INC_ENV_H */
