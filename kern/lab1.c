#include <kern/pmap.h>

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