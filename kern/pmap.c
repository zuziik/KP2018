/*
 * ----------------------------------------------------------------------------
 * LAB 1
 * Kernel Programming 2018 @VU
 * by Matthijs Jansen and Zuzana Hromcova
 * Version from Friday, September 7th
 * ----------------------------------------------------------------------------
 * 
 * Remarks:
 * 
 * This version works with normal pages and huge pages.
 * In this version lab 1 should be completely and correctly implemented.
 *
 * There are 3 new helper functions at the top of this file.
 *  - initial merge: after page_init(), tries to create as many huge pages
 *    as possible in stead of a lot of normal pages
 *  - delete from free: when using alloc_page(), delete the page you want
 *    to alloc from the free list
 *  - merge after free: if a normal page is freed in free_page(), this function
 *    will check if the potential huge page it resides in can be created.
 *
 * Only 1 free list is used with markers to differentiate normal and huge pages
 * from each other, see /inc/paging.h. This is done mostly because the test cases
 * at the bottom of this file can only handle 1 free list and rewriting those test
 * cases would be much work.
 *
 * The free list is also transformed in a doubly linked list (so with next and previous)
 * since this would make a 1 free list implementation much easier.d
 *
 * An is_available flag is also used to check if a page is in the free list or not 
 * for a speed boost.
 */

/* See COPYRIGHT for copyright information. */

#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/paging.h>

#include <inc/x86-64/asm.h>

#include <kern/pmap.h>

/* These variables are set in mem_init() */
size_t npages;
struct page_info *pages;                 /* Physical page state array */
static struct page_info *page_free_list; /* Free list of physical pages */

/***************************************************************
 * Set up memory mappings above UTOP.
 ***************************************************************/

static void check_page_free_list(bool only_low_memory);
static void check_page_alloc(void);


// After initializing the pages and page_free_list array, 
// check the whole memory to see if huge pages can be created
// by merging many small pages. Only executes once at the start.
void initial_merge(struct boot_info *boot_info) {
    struct page_info *cur;
    struct page_info *page;
    struct mmap_entry *entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);
    uintptr_t start_huge = 0;
    uintptr_t pa = 0;
    uintptr_t prev = 0;
    size_t i;    
    int m;
    int start = 0;

    // Loop through all pages
    for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
        prev = 0;
        start_huge = 0;
        if (entry->type != MMAP_FREE) {
            continue;
        }
        for (pa = entry->addr; pa < entry->addr + entry->len; pa += PAGE_SIZE) {
            // Skip if segment is too short
            if (entry->len < SMALL_PAGES_IN_HUGE) {
                break;
            }

            // Find start of huge page
            if (pa % (SMALL_PAGES_IN_HUGE * PAGE_SIZE) == 0) {
                start = 1;
                start_huge = pa;
            }

            // Only start searching when the beginning of a block has been found
            // Check if each small page is free
            page = pa2page(pa);
            if (start) {
                // Not next to each other (aligned) or not in free list
                if (page->is_available != 1 || 
                    (start_huge != pa && pa != prev + PAGE_SIZE)) {
                    start = 0;
                }
            }

            // Reached end of huge page and everything is free, merge
            // Mark first entry as huge and delete rest from free
            if (start && pa == start_huge + (SMALL_PAGES_IN_HUGE - 1) * PAGE_SIZE) {
                // Delete small pages from free list except the first
                for (m = start_huge + PAGE_SIZE; m < start_huge + SMALL_PAGES_IN_HUGE *
                     PAGE_SIZE; m += PAGE_SIZE) {
                    cur = pa2page(m);
                    cur->is_available = 0;

                    // Is first entry
                    if (cur->previous == NULL) {
                        page_free_list = cur->pp_link;
                        if (page_free_list != NULL) {
                            page_free_list->previous = NULL;
                        }
                    } else {
                        // Fix links
                        (cur->previous)->pp_link = cur->pp_link;
                        if (cur->pp_link != NULL) {
                            (cur->pp_link)->previous = cur->previous;
                        }
                    }
                    cur->pp_link = NULL;
                    cur->previous = NULL;
                }
                // First entry was not deleted, is new huge entry
                pa2page(start_huge)->is_huge = 1;
            }
            prev = pa;
        }
    }
}


// Search for a normal or huge page in the free list
// If the right type is found, remove it and return it
struct page_info *delete_from_free(int ishuge) {
    struct page_info *cur = page_free_list;

    // Search for free page
    while (cur) {
        if (cur->is_huge == ishuge) {
            break;
        } 
        cur = cur->pp_link;
    }

    // No page found, out of memory or split huge if small
    if (cur == NULL) {
        return NULL;
    } 

    // Use first entry in list
    if (cur->previous == NULL) {
        page_free_list = page_free_list->pp_link;
        if (page_free_list != NULL) {
            page_free_list->previous = NULL;
        }
    } 
    // Not first entry
    else {
        (cur->previous)->pp_link = cur->pp_link;
        if (cur->pp_link != NULL) {
            (cur->pp_link)->previous = cur->previous;
        }
    }

    cur->is_available = 0;
    cur->pp_link = NULL;
    cur->previous = NULL;
    return cur;
}


// After freeing a small page, try to merge it with free small page around 
// it (if possible) to create a large page.
void merge_after_free(struct page_info *target) {
    struct page_info* huge_start = NULL;
    struct page_info* cur = NULL;
    int addr = page2pa(target);
    int i;

    // Find the beginning of the potential huge page the freed 
    // small page is in
    if (addr % (SMALL_PAGES_IN_HUGE * PAGE_SIZE) == 0) {
        huge_start = target;
    } else {
        while (addr % (SMALL_PAGES_IN_HUGE * PAGE_SIZE) != 0) {
            addr -= PAGE_SIZE;
        }
        huge_start = pa2page(addr);
    }

    // Check if the potential huge page is completely free
    cur = huge_start;
    for (i = 0; i < SMALL_PAGES_IN_HUGE; ++i) {
        // Check if addresses are next to each other
        if (cur == NULL || (cur != huge_start && 
            page2pa(cur->previous) + PAGE_SIZE != page2pa(cur)) ||
            cur->is_available != 1) {
            huge_start = NULL;
            break;
        }

        // Next in pages array
        cur = &cur[1];
    }

    // Create free huge page and remove all small pages from free list except the first
    if (huge_start != NULL) {
        cur = &huge_start[1];
        for (i = 1; i < SMALL_PAGES_IN_HUGE; ++i) {
            // First in free list
            if (cur->previous == NULL) {
                page_free_list = page_free_list->pp_link;
                page_free_list->previous = NULL;
            } 
            // Not first entry
            else {
                (cur->previous)->pp_link = cur->pp_link;
                if (cur->pp_link != NULL) {
                    (cur->pp_link)->previous = cur->previous;
                }
            }

            cur->is_available = 0;
            cur->pp_link = NULL;
            cur->previous = NULL;
            cur = &cur[1];
        }

        // First entry was not deleted, is new huge entry
        huge_start->is_huge = 1;
    }
}


/* This simple physical memory allocator is used only while JOS is setting up
 * its virtual memory system.  page_alloc() is the real allocator.
 *
 * If n>0, allocates enough pages of contiguous physical memory to hold 'n'
 * bytes.  Doesn't initialize the memory.  Returns a kernel virtual address.
 *
 * If n==0, returns the address of the next free page without allocating
 * anything.
 *
 * If we're out of memory, boot_alloc should panic.
 * This function may ONLY be used during initialization, before the
 * page_free_list list has been set up. */
static void *boot_alloc(uint32_t n)
{
    static char *nextfree;  /* virtual address of next byte of free memory */
    char *result;

    /* Initialize nextfree if this is the first time. 'end' is a magic symbol
     * automatically generated by the linker, which points to the end of the
     * kernel's bss segment: the first virtual address that the linker did *not*
     * assign to any kernel code or global variables. */
    if (!nextfree) {
        extern char end[];
        nextfree = ROUNDUP((char *)end, PAGE_SIZE);
    }

    /* Allocate a chunk large enough to hold 'n' bytes, then update nextfree.
     * Make sure nextfree is kept aligned to a multiple of PAGE_SIZE.
     *
     * LAB 1: Your code here.
     */
    result = nextfree;
    nextfree = ROUNDUP(nextfree + n, PAGE_SIZE);

    // TODO: Out of memory
    
    return result;
}

/*
 * Set up a two-level page table:
 *    kern_pml4 is its linear (virtual) address of the root
 *
 * This function only sets up the kernel part of the address space (ie.
 * addresses >= UTOP).  The user part of the address space will be setup later.
 *
 * From UTOP to ULIM, the user is allowed to read but not write.
 * Above ULIM the user cannot read or write.
 */
void mem_init(struct boot_info *boot_info)
{
    struct mmap_entry *entry;
    uintptr_t highest_addr = 0;
    uint32_t cr0;
    size_t i, n;

    /* Find the amount of pages to allocate structs for. */
    entry = (struct mmap_entry *)((physaddr_t)boot_info->mmap_addr);

    for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
        if (entry->type != MMAP_FREE)
            continue;

        highest_addr = entry->addr + entry->len;
    }

    npages = highest_addr / PAGE_SIZE;

    /* Remove this line when you're ready to test this function. */
    // panic("mem_init: This function is not finished\n");

    /*********************************************************************
     * Allocate an array of npages 'struct page_info's and store it in 'pages'.
     * The kernel uses this array to keep track of physical pages: for each
     * physical page, there is a corresponding struct page_info in this array.
     * 'npages' is the number of physical pages in memory.  Your code goes here.
     */
    
    pages = boot_alloc(sizeof(struct page_info)*npages);
    
    for (i = 0; i < npages; i++) {
        pages[i].pp_link = NULL;
        pages[i].previous = NULL;
        pages[i].pp_ref = 0;
        pages[i].is_huge = 0;
        pages[i].is_available = 0;
    }

    /*********************************************************************
     * Now that we've allocated the initial kernel data structures, we set
     * up the list of free physical pages. Once we've done so, all further
     * memory management will go through the page_* functions. In particular, we
     * can now map memory using boot_map_region or page_insert.
     */
    page_init(boot_info);

    check_page_free_list(1);
    check_page_alloc();

    /* We will set up page tables here in lab 2. */
}

/***************************************************************
 * Tracking of physical pages.
 * The 'pages' array has one 'struct page_info' entry per physical page.
 * Pages are reference counted, and free pages are kept on a linked list.
 ***************************************************************/

/*
 * Initialize page structure and memory free list.
 * After this is done, NEVER use boot_alloc again.  ONLY use the page
 * allocator functions below to allocate and deallocate physical
 * memory via the page_free_list.
 */
void page_init(struct boot_info *boot_info)
{
    struct page_info *page;
    struct mmap_entry *entry;
    uintptr_t pa, end;
    pa = 0;
    size_t i;

    /*
     * The example code here marks all physical pages as free.
     * However this is not truly the case.  What memory is free?
     *  1) Mark physical page 0 as in use.
     *     This way we preserve the real-mode IDT and BIOS structures in case we
     *     ever need them.  (Currently we don't, but...)
     *  2) The rest of base memory, [PAGE_SIZE, npages_basemem * PAGE_SIZE) is free.
     *  3) Then comes the IO hole [IO_PHYS_MEM, EXT_PHYS_MEM), which must never be
     *     allocated.
     *  4) Then extended memory [EXT_PHYS_MEM, ...).
     *     Some of it is in use, some is free. Where is the kernel in physical
     *     memory?  Which pages are already in use for page tables and other
     *     data structures?
     *
     * Change the code to reflect this.
     * NB: DO NOT actually touch the physical memory corresponding to free
     *     pages! */
    entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);
    end = PADDR(boot_alloc(0));

    page_free_list = NULL;
    for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
        if (entry->type != MMAP_FREE) {
            continue;
        }
        for (pa = entry->addr; pa < entry->addr + entry->len; pa += PAGE_SIZE) {
            page = pa2page(pa);

            // Check which address shouldnt be free
            if (!(pa == 0 || pa == MPENTRY_PADDR || 
                 (pa >= KERNEL_LMA && pa < end))) {
                page->is_available = 1;

                // Set next and previous
                page->pp_link = page_free_list;
                if (page_free_list != NULL) {
                    page_free_list->previous = page;
                }
                page_free_list = page;
            } 
        }
    }

    // Merge the small pages to create huge pages
    initial_merge(boot_info);
}

/*
 * Allocates a physical page.  If (alloc_flags & ALLOC_ZERO), fills the entire
 * returned physical page with '\0' bytes.  Does NOT increment the reference
 * count of the page - the caller must do these if necessary (either explicitly
 * or via page_insert).
 *
 * Be sure to set the pp_link field of the allocated page to NULL so
 * page_free can check for double-free bugs.
 *
 * Returns NULL if out of free memory.
 *
 * Hint: use page2kva and memset
 *
 * 2MB huge pages:
 * Come back later to extend this function to support 2MB huge page allocation.
 * if (alloc_flags & ALLOC_HUGE), returns a huge physical page of 2MB size.
 */
struct page_info *page_alloc(int alloc_flags)
{
    struct page_info *page;
    struct page_info *tmp;
    int i;
    
    // If out of memory, return NULL
    if (page_free_list == NULL) {
        return NULL;
    }

    // Try to allocate a huge page
    if (alloc_flags & ALLOC_HUGE) {
        page =  delete_from_free(1);
        if (page == NULL)
            return NULL;
    }
    // Try to allocate a small page
    else {
        page = delete_from_free(0);

        // If there is no small page available,
        // we will split a huge page into several small pages
        if (page == NULL) {
            // Save first page
            tmp = page_free_list;
            page_free_list = page_free_list->pp_link;

            if (page_free_list != NULL) {
                page_free_list->previous = NULL;
            }

            tmp->is_huge = 0;
            tmp->is_available = 0;
            tmp->pp_link = NULL;
            tmp->previous = NULL;

            // Add the rest of the pages to the free pages pool.
            // They are stored in an array so we can do it like this.
            page = tmp;
            for (i = 1; i < SMALL_PAGES_IN_HUGE; i++) {
                // Prepend
                page[i].pp_link = page_free_list;
                if (page_free_list != NULL) {
                    page_free_list->previous = &page[i];
                } 
                page_free_list = &page[i];

                page[i].is_huge = 0;
                page[i].is_available = 1;
            }
            page_free_list->previous = NULL;

            // First page of huge page is allocated
            page = tmp;
        }
    }

    // Initialize with zeros
    if (alloc_flags & ALLOC_ZERO) {
        if (alloc_flags & ALLOC_HUGE)
            memset(page2kva(page), 0, PAGE_SIZE * SMALL_PAGES_IN_HUGE);
        else
            memset(page2kva(page), 0, PAGE_SIZE);
    }

    return page;
}

/*
 * Return a page to the free list.
 * (This function should only be called when pp->pp_ref reaches 0.)
 */
void page_free(struct page_info *pp)
{
    struct page_info* tmp;
    struct page_info* prev;
    int i, j;
    j = 0;
    
    /* Fill this function in
     * Hint: You may want to panic if pp->pp_ref is nonzero or
     * pp->pp_link is not NULL. */
    if (pp->pp_link != NULL) {
        panic("Failed to free a page with pp_link != NULL");
    }
    if (pp->pp_ref != 0) {
        panic("Failed to free a page with nonzero refcount");        
    }
    if (pp->is_available == 1) {
        panic("Attempt to double-free a page failed");
    }

    // Add page to free list
    if (page_free_list == NULL) {
        page_free_list = pp;
        pp->pp_link = NULL;
    } else {
        page_free_list->previous = pp;
        pp->pp_link = page_free_list;
        page_free_list = pp;
    }
    pp->previous = NULL;
    pp->is_available = 1;

    // If small page is added, check if huge page can be created
    if (!pp->is_huge) {
        merge_after_free(pp);
    }
}

/*
 * Decrement the reference count on a page,
 * freeing it if there are no more refs.
 */
void page_decref(struct page_info* pp)
{
    if (--pp->pp_ref == 0)
        page_free(pp);
}

/***************************************************************
 * Checking functions.
 ***************************************************************/

/*
 * Check that the pages on the page_free_list are reasonable.
 */
static void check_page_free_list(bool only_low_memory)
{
    struct page_info *pp;
    physaddr_t limit = only_low_memory ? 0x400000 : 0xFFFFFFFF;
    int nfree_basemem = 0, nfree_extmem = 0;
    char *first_free_page;

    if (!page_free_list)
        panic("'page_free_list' is a null pointer!");

    if (only_low_memory) {
        /* Move pages with lower addresses first in the free list, since
         * entry_pgdir does not map all pages. */
        struct page_info *pp1, *pp2;
        struct page_info **tp[2] = { &pp1, &pp2 };
        for (pp = page_free_list; pp; pp = pp->pp_link) {
            int pagetype = page2pa(pp) >= limit;
            *tp[pagetype] = pp;
            tp[pagetype] = &pp->pp_link;
        }
        *tp[1] = 0;
        *tp[0] = pp2;
        page_free_list = pp1;
    }

    /* if there's a page that shouldn't be on the free list,
     * try to make sure it eventually causes trouble. */
    for (pp = page_free_list; pp; pp = pp->pp_link)
        if (page2pa(pp) < limit)
            memset(page2kva(pp), 0x97, 128);

    first_free_page = (char *) boot_alloc(0);
    for (pp = page_free_list; pp; pp = pp->pp_link) {
        /* check that we didn't corrupt the free list itself */
        assert(pp >= pages);
        assert(pp < pages + npages);
        assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

        /* check a few pages that shouldn't be on the free list */
        assert(page2pa(pp) != 0);
        assert(page2pa(pp) != IO_PHYS_MEM);
        assert(page2pa(pp) != EXT_PHYS_MEM - PAGE_SIZE);
        assert(page2pa(pp) != EXT_PHYS_MEM);
        assert(page2pa(pp) < EXT_PHYS_MEM || (char *) page2kva(pp) >= first_free_page);

        if (page2pa(pp) < EXT_PHYS_MEM)
            ++nfree_basemem;
        else
            ++nfree_extmem;
    }

    assert(nfree_basemem > 0);
    assert(nfree_extmem > 0);
}

/*
 * Check the physical page allocator (page_alloc(), page_free(),
 * and page_init()).
 */
static void check_page_alloc(void)
{
    struct page_info *pp, *pp0, *pp1, *pp2;
    struct page_info *php0, *php1, *php2;
    int nfree, total_free;
    struct page_info *fl;
    char *c;
    int i;

    if (!pages)
        panic("'pages' is a null pointer!");

    /* check number of free pages */
    for (pp = page_free_list, nfree = 0; pp; pp = pp->pp_link)
        ++nfree;

    total_free = nfree;

    /* should be able to allocate three pages */
    pp0 = pp1 = pp2 = 0;
    assert((pp0 = page_alloc(0)));
    assert((pp1 = page_alloc(0)));
    assert((pp2 = page_alloc(0)));

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);
    assert(page2pa(pp0) < npages*PAGE_SIZE);
    assert(page2pa(pp1) < npages*PAGE_SIZE);
    assert(page2pa(pp2) < npages*PAGE_SIZE);

    /* temporarily steal the rest of the free pages */
    fl = page_free_list;
    page_free_list = 0;

    /* should be no free memory */
    assert(!page_alloc(0));

    /* free and re-allocate? */
    page_free(pp0);
    page_free(pp1);
    page_free(pp2);
    pp0 = pp1 = pp2 = 0;
    assert((pp0 = page_alloc(0)));
    assert((pp1 = page_alloc(0)));
    assert((pp2 = page_alloc(0)));
    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);
    assert(!page_alloc(0));

    /* test flags */
    memset(page2kva(pp0), 1, PAGE_SIZE);
    page_free(pp0);
    assert((pp = page_alloc(ALLOC_ZERO)));
    assert(pp && pp0 == pp);
    c = page2kva(pp);
    for (i = 0; i < PAGE_SIZE; i++)
        assert(c[i] == 0);

    /* give free list back */
    page_free_list = fl;

    /* free the pages we took */
    page_free(pp0);
    page_free(pp1);
    page_free(pp2);

    /* number of free pages should be the same */
    for (pp = page_free_list; pp; pp = pp->pp_link)
        --nfree;
    assert(nfree == 0);

    cprintf("[4K] check_page_alloc() succeeded!\n");

    // /* test allocation of huge page */
    pp0 = pp1 = php0 = 0;
    assert((pp0 = page_alloc(0)));
    assert((php0 = page_alloc(ALLOC_HUGE)));
    assert((pp1 = page_alloc(0)));
    assert(pp0);
    assert(php0 && php0 != pp0);
    assert(pp1 && pp1 != php0 && pp1 != pp0);
    assert(0 == (page2pa(php0) % 512*PAGE_SIZE));
    if (page2pa(pp1) > page2pa(php0)) {
        assert(page2pa(pp1) - page2pa(php0) >= 512*PAGE_SIZE);
    }

    /* free and reallocate 2 huge pages */
    page_free(php0);
    page_free(pp0);
    page_free(pp1);
    php0 = php1 = pp0 = pp1 = 0;
    assert((php0 = page_alloc(ALLOC_HUGE)));
    assert((php1 = page_alloc(ALLOC_HUGE)));

    /* Is the inter-huge-page difference right? */
    if (page2pa(php1) > page2pa(php0)) {
        assert(page2pa(php1) - page2pa(php0) >= 512*PAGE_SIZE);
    } else {
        assert(page2pa(php0) - page2pa(php1) >= 512*PAGE_SIZE);
    }

    /* free the huge pages we took */
    page_free(php0);
    page_free(php1);

    /* number of free pages should be the same */
    nfree = total_free;
    for (pp = page_free_list; pp; pp = pp->pp_link)
        --nfree;
    assert(nfree == 0);

    cprintf("[2M] check_page_alloc() succeeded!\n");
}
