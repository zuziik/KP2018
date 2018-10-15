#pragma once

#include <inc/x86-64/paging.h>

/* At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
 * IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
 * at physical address EXTPHYSMEM. */
#define IO_PHYS_MEM   0x0A0000
#define EXT_PHYS_MEM  0x100000

/* Physical address of startup code for non-boot CPUs (APs) */
#define MPENTRY_PADDR   0x7000

/* User accessible page tables. These are the only addresses at which they are
 * contiguous.
 */
#ifdef __ASSEMBLER__
#define USER_PML4 0xFFFFFFFFFFFFF000
#define USER_PDPT 0xFFFFFFFFFFE00000
#define USER_PD   0xFFFFFFFFC0000000
#define USER_PT   0xFFFFFF8000000000
#else
#define USER_PML4 UINT64_C(0xFFFFFFFFFFFFF000)
#define USER_PDPT UINT64_C(0xFFFFFFFFFFE00000)
#define USER_PD   UINT64_C(0xFFFFFFFFC0000000)
#define USER_PT   UINT64_C(0xFFFFFF8000000000)
#endif

/* Memory mapped I/O. */
#define MMIO_LIM USER_PT
#define MMIO_BASE (MMIO_LIM - PDPT_SPAN)

/* Kernel stack. */
#define KSTACK_TOP MMIO_BASE
#define KSTACK_SIZE (8 * PAGE_SIZE)
#define KSTACK_GAP (8 * PAGE_SIZE)

/* Kthreads stacks */
#define KTHREAD_STACK_TOP KSTACK_TOP - (KSTACK_SIZE + KSTACK_GAP) * NCPU
#define KTHREAD_STACK_SIZE (2 * PAGE_SIZE)
#define KTHREAD_STACK_GAP (PAGE_SIZE)
#define KTHREADS_BOT (KTHREAD_STACK_TOP - (KTHREAD_STACK_SIZE + KTHREAD_STACK_GAP) * MAX_KTHREADS)

/* Kthreads lists */
#define KTHREADS (KTHREADS_BOT - ROUNDUP(MAX_KTHREADS * sizeof(struct kthread), PAGE_SIZE))

#define SWAPSLOTS (KTHREADS - PDPT_SPAN)

#define POOL_SWAPPED (SWAPSLOTS - PDPT_SPAN)
#define POOL_MAPPING (POOL_SWAPPED - PDPT_SPAN)
#define POOL_ENV_MAPPING (POOL_MAPPING - PDPT_SPAN)

#define MAX_POOL_SIZE (PDPT_SPAN/PAGE_SIZE)

/* User address space limit. */
#ifdef __ASSEMBLER__
#define USER_LIM 0x800000000000
#else
#define USER_LIM UINT64_C(0x800000000000)
#endif

/* User pages (read-only). */
#define USER_PAGES (USER_LIM - PDPT_SPAN)

/* User environments (read-only). */
#define USER_ENVS (USER_PAGES - PDPT_SPAN)

/* VMA lists */
#define USER_VMAS (USER_ENVS - PDPT_SPAN)

/* User stacks. */
#define USER_TOP USER_ENVS
#define UXSTACK_TOP USER_TOP
#define USTACK_TOP (UXSTACK_TOP - 2 * PAGE_SIZE)

/* Used for temporary page mappings. Typed as a void pointer as a convenience.
 */
#define UTEMP ((void *)PAGE_SIZE)

