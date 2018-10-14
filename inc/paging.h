#pragma once

#include <inc/x86-64/paging.h>

#ifndef __ASSEMBLER__
/*
 * Page descriptor structures, mapped at UPAGES.
 * Read/write to the kernel, read-only to user programs.
 *
 * Each struct PageInfo stores metadata for one physical page.
 * Is it NOT the physical page itself, but there is a one-to-one
 * correspondence between physical pages and struct PageInfo's.
 * You can map a struct PageInfo * to the corresponding physical address
 * with page2pa() in kern/pmap.h.
 */
struct page_info {
    /* Double-linked list for page_faults, used for page reclaiming */
    struct page_info *fault_next;
    struct page_info *fault_prev;

    /* Next and previous page on the free list. */
    struct page_info *pp_link;
    struct page_info *previous;

    /* pp_ref is the count of pointers (usually in page table entries)
     * to this page, for pages allocated using page_alloc.
     * Pages allocated at boot time using pmap.c's
     * boot_alloc do not have valid reference count fields. */
    uint16_t is_huge;
    uint16_t pp_ref;

    /* Is in the free list or not */
    uint16_t is_available;

    /* Linked list of VMAs that contain(ed) VAs mapping this physical page */
    struct mapped_va *reverse_mapping;
};
#endif /* !__ASSEMBLER__ */

