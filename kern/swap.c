#include <kern/swap.h>
#include <kern/vma.h>
#include <kern/pmap.h>


// struct page_fault *page_faults = NULL;
struct page_info *page_fault_head = NULL;
struct page_info *page_fault_tail = NULL;


/**
* Initialization of swapping functionality. Prepares control
* datastructure to keep track on changes on the disk.
*/
void swap_init() {

	// Allocating memory for disk_slots data structure
	// (information about free/used sectors on disk)

    uintptr_t vi;
	uintptr_t va_start;
    uintptr_t va_end;
    struct page_info *p;
    uint32_t size;
    int i;

	nswapslots = ide_num_sectors() / (PAGE_SIZE/SECTSIZE);
	cprintf("[SWAP INIT] We can fit %d pages on swap disk\n", nswapslots);

	size = sizeof(struct swap_slot)*nswapslots;

    p = NULL;

    va_start = (uintptr_t) DISKSLOTS;
    va_end = ROUNDUP(va_start + size, PAGE_SIZE);


    for (vi = va_start; vi < va_end; vi += PAGE_SIZE) {
        if (!(p = page_alloc(ALLOC_ZERO)))
            panic("Couldn't allocate memory for swap disk structure");
        page_insert(kern_pml4, p, (void *)vi, PAGE_WRITE | PAGE_USER);
    }

	swap_slots = (struct swap_slot *) va_start;

	// Initializing the data structure
	for (i = 0; i < nswapslots; i++) {
        swap_slots[i].is_used = 0;
        swap_slots[i].reverse_mapping = NULL;
        swap_slots[i].next = (i == nswapslots - 1) ? NULL : &swap_slots[i+1];
        swap_slots[i].prev = (i == 0) ? NULL : &swap_slots[i-1];
	}

	// Marking all disk slots as free, thus putting them in the free slots list
	free_swap_slots = swap_slots;
}

/** 
* Removes head from the free_disk_slots list
* Returns disk slot or NULL
*/
struct swap_slot *alloc_swap_slot() {
	if (free_swap_slots == NULL) {
		return NULL;
	}

	struct swap_slot *slot = free_swap_slots;
	free_swap_slots = free_swap_slots->next;
	free_swap_slots->prev = NULL;
	slot->next = NULL;

	return slot;
}

/**
* Marks the swap space slot as free, i.e. adds it to the free list
*/
void free_swap_slot(struct swap_slot *slot) {
	slot->next = free_swap_slots;
	slot->prev = NULL;
	free_swap_slots->prev = slot;
	free_swap_slots = slot;
}

/**
* Swaps out the physical page on a disk.
* Returns 1 (success) / 0 (fail)
*/
int swap_out(struct page_info *p) {
	// 1.Copy page on a disk
	// Blocking now
	// Maybe we don't want any local variables
	// Always a small page
	int i;
	struct swap_slot *slot;
	struct env_va_mapping *env_va_mapping;
	struct va_mapping *va_mapping;

	lock_swapslot();
	slot = alloc_swap_slot();

	// No swap space available, return fail
	if (slot == NULL) {
		unlock_swapslot();
		return 0;
	}

	ide_start_write(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++)
		while (!ide_is_ready());
		ide_write_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);

	// 2. Remove VMA list from page_info and map it to swap structure
	slot->reverse_mapping = p->reverse_mapping;
	p->reverse_mapping = NULL;

	// 3. Zero-out all entries pointing to this physical page and
	// decrease reference count on the page
	// 4. Add the slot to every VMA swapped_pages list
	env_va_mapping = slot->reverse_mapping;
	while (env_va_mapping != NULL) {
		va_mapping = env_va_mapping->list;
		while (va_mapping != NULL) {
			// TODO Problem with invalidating TLB cache, see tlb_invalidate() implementation
			page_remove(env_va_mapping->e->env_pml4, va_mapping->va);
			vma_add_swapped_page(env_va_mapping->e, va_mapping->va, slot);
			va_mapping = va_mapping->next;
		}
		env_va_mapping = env_va_mapping->next;
	}

	// 5. Free the physical page
	if (p->pp_ref != 0) {
		panic("[SWAP_OUT] Didn't remove all the mappings to the swapped out page");
	}
	p->pp_ref = 1;
	page_decref(p);

	unlock_swapslot();
	return 1;
}

/**
* Allocates a new physical page and copies it back from disk.
* + There will be another function that calls this one, which
* first walks the swap_slots array to search for the slot which
* contains the swapped out VA+env / PTE.
* Returns 1 (success) / 0 (fail)
*/
int swap_in(struct swap_slot *slot) {

	int i;
	struct env_va_mapping *env_va_mapping;
	struct va_mapping *va_mapping;

	// 1. Allocate a page
	struct page_info *p = page_alloc(0);
	if (p == NULL)
		return 0;

	// 2. Copy data back from the disk
	lock_swapslot();
	ide_start_read(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++)
		while (!ide_is_ready());
		ide_read_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);

	// 3. Free the swap space
	free_swap_slot(slot);

	// 4. Walk the saved reverse mappings and map the page everywhere
	// it was mapped before + increase the refcount with each mapping
	// 5. Remove the slot from swapped_pages list of each VMA
	env_va_mapping = slot->reverse_mapping;
	while (env_va_mapping != NULL) {
		va_mapping = env_va_mapping->list;
			while (va_mapping != NULL) {
			page_insert(env_va_mapping->e->env_pml4, va_mapping->va, p, va_mapping->perm);
			vma_remove_swapped_page(env_va_mapping->e, va_mapping->va);
			va_mapping = va_mapping->next;
		}
		env_va_mapping = env_va_mapping->next;
	}

	// 6. Correctly set the reverse mappings to the new page
	p->reverse_mapping = slot->reverse_mapping;
	slot->reverse_mapping = NULL;

	unlock_swapslot();
	return 1;
}

/**
* Remove a reverse mapping from a physical page
*/
void remove_reverse_mapping(struct env *e, void *va, struct page_info *page) {

	// TODO ZUZANA
	// if (page == NULL)
	// 	page = pa2page(page_walk(e->env_pml4, va, 0));

	if (page == NULL)
		return;

	struct va_mapping *curr_va_mapping;	
	struct va_mapping *prev_va_mapping;	
	struct env_va_mapping *curr_env_va_mapping = page->reverse_mapping;
	struct env_va_mapping *prev_env_va_mapping = NULL;

	while ((curr_env_va_mapping != NULL) && (curr_env_va_mapping->e != e)) {
		prev_env_va_mapping = curr_env_va_mapping;
		curr_env_va_mapping = curr_env_va_mapping->next;
	}

	// Mapping for env is not there, nothing to remove, return
	if (curr_env_va_mapping == NULL)
		return;

	curr_va_mapping = curr_env_va_mapping->list;
	prev_va_mapping = NULL;

	while ((curr_va_mapping != NULL) && (curr_va_mapping->va != va)) {
		prev_va_mapping = curr_va_mapping;
		curr_va_mapping = curr_va_mapping->next;
	}

	// Mapping for this env and va is not there, nothing to remove, return
	if (curr_va_mapping == NULL)
		return;

	// VA is the first in the list
	if (prev_va_mapping == NULL)
		curr_env_va_mapping->list = curr_va_mapping->next;
	else
		prev_va_mapping->next = curr_va_mapping->next;

	// TODO ZUZANA free(curr_va_mapping)

	// All mappings of the environment were freed, remove the list for the environment
	if (curr_env_va_mapping->list == NULL) {
		if (prev_env_va_mapping == NULL)
			page->reverse_mapping = curr_env_va_mapping->next;
		else
			prev_env_va_mapping->next = curr_env_va_mapping->next;
		// TODO ZUZANA free(curr_env_va_mapping)
	}
}

/**
* Remove reverse mappings of this environment from all pages
* Called from env_free()
*/
void env_remove_reverse_mappings(struct env *e) {
	int i;

	struct va_mapping *curr_va_mapping;
	struct va_mapping *next_va_mapping;
	struct env_va_mapping *curr_env_va_mapping;
	struct env_va_mapping *prev_env_va_mapping;

	// Remove mappings for each page
	for (i = 0; i < npages; i++) {
		curr_env_va_mapping = pages[i].reverse_mapping;
		prev_env_va_mapping = NULL;

		// Find list of mappings for the specified environment
		while ((curr_env_va_mapping != NULL) && (curr_env_va_mapping->e != e)) {
			prev_env_va_mapping = curr_env_va_mapping;
			curr_env_va_mapping = curr_env_va_mapping->next;
		}
		// Mapping for env is not there, nothing to remove, continue with next page
		if (curr_env_va_mapping == NULL)
			continue;

		curr_va_mapping = curr_env_va_mapping->list;

		while (curr_va_mapping != NULL) {
			next_va_mapping = curr_va_mapping->next;
			// free(curr_va_mapping); TODO ZUZANA
		}

		// Remove env list from the page mappings
		if (prev_env_va_mapping == NULL)
			pages[i].reverse_mapping = curr_env_va_mapping->next;
		else
			prev_env_va_mapping->next = curr_env_va_mapping->next;
		// free(curr_env_va_mapping); TODO ZUZANA

	}
}


/**
* Adds a reverse mapping to a physical page
*/
void add_reverse_mapping(struct env *e, void *va, struct page_info *page, int perm) {

	// TODO ZUZANA change how it's allocated
	struct va_mapping new_va_mapping;	
	// TODO ZUZANA change how to alloc + this one only if needed	
	struct env_va_mapping new_env_va_mapping;

	struct env_va_mapping *tmp_env_va_mapping;

	new_va_mapping.va = va;
	new_va_mapping.perm = perm;

	tmp_env_va_mapping = page->reverse_mapping;

	while ((tmp_env_va_mapping->e != e) && (tmp_env_va_mapping != NULL))
		tmp_env_va_mapping = tmp_env_va_mapping->next;

	// Nothing is mapped for the env yet
	if (tmp_env_va_mapping == NULL) {
		tmp_env_va_mapping = &new_env_va_mapping;
		tmp_env_va_mapping->e = e;
		tmp_env_va_mapping->next = page->reverse_mapping;
		page->reverse_mapping = tmp_env_va_mapping;
	}

	new_va_mapping.next = tmp_env_va_mapping->list;
	tmp_env_va_mapping->list = &new_va_mapping;
}

/**
* Searches swapped pages list of the given VMA and looks for the given VA.
* If found, returns pointer to the swap slot where the page is swapped out.
*/
struct swap_slot *vma_lookup_swapped_page(struct vma *vma, void *va) {
	struct swapped_va *swapped = vma->swapped_pages;
	while ((swapped != NULL) && (swapped->va != va)) {
		swapped = swapped->next;
	}
	if (swapped == NULL) {
		return NULL;
	}
	return swapped->slot;
}
 
/**
* Adds the VA and swap slot to the corresponding VMA list of swapped out pages.
*/     
void vma_add_swapped_page(struct env *e, void *va, struct swap_slot *slot) {
	struct vma *vma = vma_lookup(e, va);
	if (vma == NULL)
		return;

	// TODO: Is THIS how we want to allocate it? Probably not, but then how?
	struct swapped_va swapped;
	swapped.va = va;
	swapped.slot = slot;
	swapped.next = vma->swapped_pages;

	vma->swapped_pages = &swapped;
}

/**
* Removes the VA from the corresponding VMA list of swapped out pages.
*/
void vma_remove_swapped_page(struct env *e, void *va) {
	struct vma *vma = vma_lookup(e, va);
	if (vma == NULL)
		return;

	struct swapped_va *curr = vma->swapped_pages;
	struct swapped_va *prev = vma->swapped_pages;

	// First one? Remove head
	// TODO: Is it ok that we just let the removed structure go? No free?
	if (curr->va == va) {
		vma->swapped_pages = curr->next;
		return;
	}

	curr = curr->next;

	while (curr != NULL) {
		if (curr->va == va) {
			prev->next = curr->next;
			return;
		}
		prev = curr;
		curr = curr->next;
	}

	panic("swapped_va not in vma list\n");
}


/**
* Initializes freepages counter. Called during boot.
*/
void set_nfreepages(size_t num) {
	lock_nfreepages();
	nfreepages = num;
	unlock_nfreepages();
}

/**
* Returns 1 if num free pages are available, using
* the freepages counter.
* Called just before allocation and from a kernel
* swap thread.
*/
int available_freepages(size_t num) {
	lock_nfreepages();
	int res = nfreepages >= num;
	unlock_nfreepages();
	return res;
}

/**
* Increments the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages.
* This function is called from page_free().
*/
void inc_nfreepages(int huge) {
	lock_nfreepages();
	if (huge)
		nfreepages += SMALL_PAGES_IN_HUGE;
	else
		nfreepages++;
	unlock_nfreepages();
}

/**
* Decrements the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages.
* This function is called from page_alloc().
*/
void dec_nfreepages(int huge) {
	lock_nfreepages();
	if (huge)
		nfreepages -= SMALL_PAGES_IN_HUGE;
	else
		nfreepages--;
	unlock_nfreepages();
}

/**
* Check if the page is part of the page fault list
* If so, remove it from the list and fix the pointers and globals
*/
void page_fault_remove(struct page_info *page) {
	// Is the only element in the list, only used to remove from page_free
	if (page == page_fault_head && page == page_fault_tail) {
		page_fault_head = NULL;
		page_fault_tail = NULL;
	}
	// Remove if tail, only use to remove from page_free
	else if (page == page_fault_tail) {
		page_fault_tail = page_fault_tail->fault_next;
		page->fault_next = NULL;
	}

	// Length list is > 1
	if (page->fault_next != NULL || page->fault_prev != NULL) {
		// Page is head
		if (page == page_fault_head) {
			page_fault_head = page->fault_prev;
			page_fault_head->fault_next = NULL;
			page->fault_prev = NULL;
		} 
		// Page is not tail, remove from middle
		else if (page != page_fault_tail) {
			(page->fault_next)->fault_prev = page->fault_prev;
			(page->fault_prev)->fault_next = page->fault_next;
			page->fault_next = NULL;
			page->fault_prev = NULL;
		}
	}
}

/**
* Inserts the faulting VA at the end of a faulting
* pages queue (first FIFO, later CLOCK).
* This function is called from the page fault handler,
* but only after a new page is allocated (i.e. not
* if the page fault was a permission issue).
* Expects fault_va to be aligned.
* Clock will be enforced in swap_pages()
*/
void page_fault_queue_insert(uintptr_t fault_va) {
	int lock = swap_lock_pagealloc();

	struct vma *vma;
	struct page_info *page;
	physaddr_t *pt_entry = NULL;
	page = page_lookup(curenv->env_pml4, (void *) fault_va, &pt_entry);

	// Corner cases: empty list
	if (page_fault_head == NULL && page_fault_tail == NULL) {
		page_fault_head = page;
		page_fault_tail = page;
	}
	// Remove from the list if already there and append to the back
	else if (page != page_fault_tail) {
		// Check if page was already in the list and if so, remove (COW)
		page_fault_remove(page);

		// Append to the back
		page_fault_tail->fault_prev = page;
		page->fault_next = page_fault_tail;
		page->fault_prev = NULL;
		page_fault_tail = page;
	}

	swap_unlock_pagealloc(lock);
}

// Pop the head of the page fault list
// TODO: implement CLOCK
struct page_info *page_fault_pop_head() {
	struct page_info *page;

	// Page fault list is empty
	if (page_fault_head == NULL) {
		return NULL;
	}

	// Pop head and update
	page = page_fault_head;
	page_fault_head = page_fault_head->fault_prev;

	// Check corner cases: empty list
	if (page_fault_head != NULL) {
		page_fault_head->fault_next = NULL;
	} else {
		// If head is NULL, tail is also NULL
		page_fault_tail = NULL;
	}

	page->fault_next = NULL;
	page->fault_prev = NULL;

	return page;
}

/**
* Swaps out pages from the LRU list (FIFO/CLOCK) on the disk, until
* we have at least FREEPAGE_THRESHOLD pages available.
* Uses swap_out() function for each of the swapped pages.
* Called both from direct reclaiming and periodic reclaiming function.
* Returns 1 (success) / 0 (fail)
*/
int swap_pages() {

	// Get the amount of pages to free
	lock_nfreepages();
	int to_free = FREEPAGE_THRESHOLD + FREEPAGE_OVERTHRESHOLD - nfreepages;
	lock_nfreepages();

	int i;
	struct page_info *page;

	// Pop to_free times and swap out
	int lock = swap_lock_pagealloc();
	for (i = 0; i < to_free; i++) {
		page = page_fault_pop_head();

		// If something doesnt work, do oom killing
		if (page == NULL || !swap_out(page)) {
			swap_unlock_pagealloc(lock);
			return 0;
		}
	}
	swap_unlock_pagealloc(lock);
	return 1;
}

/**
* Returns the number of swapped out pages for the specified envuronment.
* Used by the oom_kill_process() function.
*/
uint32_t count_swapped_pages(struct env* e) {
	struct vma *vma;
	struct swapped_va *swapped_page;
	uint32_t num_swap = 0;

   	vma = e->vma;

	while (vma != NULL) {
		swapped_page = vma->swapped_pages;
		while (swapped_page != NULL) {
			num_swap++;
			swapped_page = swapped_page->next;
		}
		vma = vma->next;
    }

    return num_swap;
}

/**
* Returns the number of allocated pages for the specified envuronment.
* (only mapped pages, not page tables)
* Used by the oom_kill_process() function.
*/
uint32_t count_allocated_pages(struct env* e) {
	struct env_va_mapping *env_va_mapping;
	uint32_t num_alloc = 0;
	int i;

	for (i = 0; i < npages; i++) {
		env_va_mapping = pages[i].reverse_mapping;
		while ((env_va_mapping != NULL) && (env_va_mapping->e != e)) {
			env_va_mapping = env_va_mapping->next;
		}
		// This page is used by the environment - increment the counter
		if (env_va_mapping != NULL)
			num_alloc++;
	}

	return num_alloc;
}

/**
* Returns the number of physical pages used by the specified envuronment
* for page table tree.
* Used by the oom_kill_process() function.
*/
uint32_t count_table_pages(struct env* e) {
	uint32_t num_tables = 0;
	int s, t, u;
	struct page_table *pdp, *pd, *pt;
	struct page_table *pml4 = e->env_pml4;
	physaddr_t entry;

	if (pml4 == NULL)
		return num_tables;

	num_tables++;

	for (s = 0; s < PAGE_TABLE_ENTRIES; s++) {
		entry = pml4->entries[s];
		if (!(entry & PAGE_PRESENT))
			continue;
		num_tables++;

		pdp = (struct page_table *)KADDR(PAGE_ADDR(entry));
		for (t = 0; t < PAGE_TABLE_ENTRIES; t++) {
			entry = pdp->entries[t];
			if (!(entry & PAGE_PRESENT))
				continue;
			num_tables++;

			pd = (struct page_table *)KADDR(PAGE_ADDR(entry));
			for (u = 0; u < PAGE_TABLE_ENTRIES; u++) {
				entry = pd->entries[u];
				if (!(entry & PAGE_PRESENT))
					continue;
				if (entry & PAGE_HUGE)
					continue;
				num_tables++;
			}
		}
	}

	return num_tables;

}

/**
* Kills the most problematic process, thus freeing its memory.
* This function is called from page_reclaim() function if the swapping
* is not possible.
* Returns 1 (success) / 0 (fail)
*/
int oom_kill_process() {
	process_to_kill = -1;	

    // Walk envs list and compute RSS for each process based on the stats 
    // stored there. Choose the process with the highest score.
	lock_env();
    for (counter = 0; counter < NENV; counter++) {
    	current_rss = (count_allocated_pages(&envs[counter]) 
    		+ count_swapped_pages(&envs[counter]) 
    		+ count_table_pages(&envs[counter])
    		) / PAGE_SIZE + npages/1000;
    	cprintf("OOM process %llx, alloc %d, score %d\n", envs[counter].env_id, current_rss);
    	if (current_rss > highest_rss) {
    		highest_rss = current_rss;
    		process_to_kill = envs[counter].env_id;
    	}
    }
    unlock_env();

    if (process_to_kill == -1) {
    	panic("OOM killer - No processes to kill!");
    }

    cprintf("OOM wants to kill %llx\n", process_to_kill);

    // Kill the process = Send an interrupt to the core the process is
    // running at. The cpu will then destroy the process.
    // !!! Don't check whether it's running first, it has to be atomic.
    // If the process is not running, the CPU will simply ignore the interrupt
    // (because it will do the kernel work so IF will be cleared) so it will
    // work anyway. If we first checked whether the process is running, it
    // maybe won't but it can be rescheduled before we kill it.

    // Problem - we don't clear env_cpunum field after a process is interrupted,
    // so it's not consistent.

    // Another problem - process can be rescheduled to another core after we read
    // env_cpunum but BEFORE we send an interrupt to the core to destroy it.
    // Can we use the env lock even in the scheduler? That way this wouldn't
    // be a problem.

    // Matthijs, do you know how to send a IPI?
    return 0;
}

/**
* On-demand page reclaiming called if we are out-of memory (in page_alloc).
* We could also be out-of-memory in boot_alloc but that would be a worse
* problem and we don't want to swap kernel pages, so we don't call this
* function from there.
* Returns 1 (success) / 0 (fail)
*/
int page_reclaim() {
	// Remove unnecessary locks
	int lock = 0;
	int res = 0;
	if (holding(&pagealloc_lock)) {
		lock++;
		unlock_pagealloc();
	}

    // First, try to swap num pages.
    if (! swap_pages()) {
    	// If not successful, go for OOM killing.
    	res = oom_kill_process();
    	if (lock) {
	    	lock_pagealloc();
	    }
	    return res;
    	if (lock) {
    		lock_pagealloc();
    	}
    }
    return 1;
}

/**
* KERNEL THREAD
* Periodically checks whether the number of free pages is above a given threshold.
* If not, swaps enough pages to have FREEPAGE_THRESHOLD free pages.
* Only one instance of this
* kernel thread is running at a time.
*/
void kthread_swap() {
    while (1) {
        if (! available_freepages(FREEPAGE_THRESHOLD))
        	swap_pages();
        kthread_interrupt();
    }
}


