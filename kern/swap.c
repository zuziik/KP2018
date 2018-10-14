#include <kern/swap.h>
#include <kern/vma.h>


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
	if (free_swap_slots == NULL)
		return NULL;

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
	struct mapped_va *mapping;
	int affected_envs[NENV];

	slot = alloc_swap_slot();

	// No swap space available, return fail
	if (slot == NULL)
		return 0;

	ide_start_write(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++)
		while (!ide_is_ready());
		ide_write_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);

	// 2. Remove VMA list from page_info and map it to swap structure
	slot->reverse_mapping = p->reverse_mapping;
	p->reverse_mapping = NULL;

	// 3. Zero-out all entries pointing to this physical page and
	// decrease reference count on the page
	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		// Problem with invalidating TLB cache, see tlb_invalidate() implementation
		page_remove(mapping->e->env_pml4, mapping->va);
		mapping = mapping->next;
	}

	// 4. Update num_swap field of each affected env (number of swapped out
	// pages, used by OOM) using inc_swapped_in_env()
	// 5. Update num_alloc field for each affected env (dec_alloc_in_env())

	for (i = 0; i < NENV; i++) {
		affected_envs[i] = 0;
	}

	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		affected_envs[env2i(mapping->e)] = 1;
		mapping = mapping->next;
	}

	for (i = 0; i < NENV; i++) {
		if (affected_envs[i]) {
			envs[i].num_swap++;
			envs[i].num_alloc--;
		}
	}

	// 6. Free the physical page
	if (p->pp_ref != 0) {
		panic("[SWAP_OUT] Didn't remove all the mappings to the swapped out page");
	}
	p->pp_ref = 1;
	page_decref(p);

	// 7. Add the slot to every VMA swapped_pages list
	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		vma_add_swapped_page(mapping->e, mapping->va, slot);
		mapping = mapping->next;
	}


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
	struct mapped_va *mapping;
	int affected_envs[NENV];

	// 1. Allocate a page
	struct page_info *p = page_alloc(0);
	if (p == NULL)
		return 0;

	// 2. Copy data back from the disk
	ide_start_read(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++)
		while (!ide_is_ready());
		ide_read_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);

	// 3. Free the swap space
	free_swap_slot(slot);

	// 4. Walk the saved reverse mappings and map the page everywhere
	// it was mapped before + increase the refcount with each mapping
	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		page_insert(mapping->e->env_pml4, mapping->va, p, mapping->perm);
		mapping = mapping->next;
	}

	// 5. Update num_swap field of each affected env (number of swapped out
	// pages, used by OOM) using dec_swapped_in_env()

	// 6. Update num_alloc field for each affected env (inc_alloc_in_env())
	for (i = 0; i < NENV; i++) {
		affected_envs[i] = 0;
	}

	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		affected_envs[env2i(mapping->e)] = 1;
		mapping = mapping->next;
	}

	for (i = 0; i < NENV; i++) {
		if (affected_envs[i]) {
			envs[i].num_swap--;
			envs[i].num_alloc++;
		}
	}

	// 7. Remove the slot from swapped_pages list of each VMA
	mapping = slot->reverse_mapping;
	while (mapping != NULL) {
		vma_remove_swapped_page(mapping->e, mapping->va);
		mapping = mapping->next;
	}

	// 8. Correctly set the reverse mappings to the new page
	p->reverse_mapping = slot->reverse_mapping;
	slot->reverse_mapping = NULL;

	return 1;
}

/**
* Remove a reverse mapping from a physical page
*/
void remove_reverse_mapping(struct env *e, void *va, struct page_info *page) {

	struct mapped_va *curr = page->reverse_mapping;
	struct mapped_va *prev = page->reverse_mapping;

	// First one? Remove head
	// Is it ok that we just let the removed structure go? No free?
	if ((curr->va == va) && (curr->e == e)) {
		page->reverse_mapping = curr->next;
		return;
	}

	curr = curr->next;

	while (curr != NULL) {
		if ((curr->va == va) && (curr->e == e)) {
			prev->next = curr->next;
			return;
		}
		prev = curr;
		curr = curr->next;
	}	
}

/**
* Remove reverse mappings of this environment from all pages
* Called from env_free()
*/
void env_remove_reverse_mappings(struct env *e) {
	//TODO
	// For this, we either must walk all pages (!),
	// or update env_free_page_tables() to have information about the environment
}


/**
* Adds a reverse mapping to a physical page
*/
void add_reverse_mapping(struct env *e, void *va, struct page_info *page, int perm) {
	struct mapped_va mapping;
	mapping.va = va;
	mapping.perm = perm;
	mapping.e = e;
	mapping.next = page->reverse_mapping;

	page->reverse_mapping = &mapping;
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
	return swapped->slot;
}
 
/**
* Adds the VA and swap slot to the corresponding VMA list of swapped out pages.
*/     
void vma_add_swapped_page(struct env *e, void *va, struct swap_slot *slot) {
	struct vma *vma = vma_lookup(e, va);
	if (vma == NULL)
		return;

	// Is THIS how we want to allocate it? Probably not, but then how?
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
	// Is it ok that we just let the removed structure go? No free?
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
}


/**
* Initializes freepages counter. Called during boot.
*/
void set_nfreepages(size_t num) {
	// TODO locking?
	nfreepages = num;
}

/**
* Returns 1 if num free pages are available, using
* the freepages counter.
* Called just before allocation and from a kernel
* swap thread.
*/
int available_freepages(size_t num) {
	// TODO locking?
	return nfreepages >= num;
}

/**
* Increments the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages.
* This function is called from page_free().
*/
void inc_nfreepages(int huge) {
	// TODO locking?
	if (huge)
		nfreepages += SMALL_PAGES_IN_HUGE;
	else
		nfreepages++;
}

/**
* Decrements the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages.
* This function is called from page_alloc().
*/
void dec_nfreepages(int huge) {
	// TODO locking?
	if (huge)
		nfreepages -= SMALL_PAGES_IN_HUGE;
	else
		nfreepages--;
}

//--------------------------------------------------------
// Counters for environments. They are separate functions
// now because I think we need locking (but maybe we can
// remove them if not)

void inc_allocated_in_env(struct env *e) {
	e->num_alloc++;
}

void dec_allocated_in_env(struct env *e) {
	e->num_alloc--;
}


void inc_swapped_in_env(struct env *e) {
	e->num_swap++;
}

void dec_swapped_in_env(struct env *e) {
	e->num_swap--;
}

void inc_tables_in_env(struct env *e) {
	e->num_tables++;
}

void dec_tables_in_env(struct env *e) {
	e->num_tables--;
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
	int lock = swap_lock_pagealloc();
	struct page_info *page;

	// Page fault list is empty
	if (page_fault_head == NULL) {
		swap_unlock_pagealloc(lock);
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

	swap_unlock_pagealloc(lock);
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
	return 0; // remove this when everything works
	int to_free = FREEPAGE_THRESHOLD + FREEPAGE_OVERTHRESHOLD - nfreepages;
	int i;
	struct page_info *page;

	// Pop to_free times and swap out
	for (i = 0; i < to_free; i++) {
		page = page_fault_pop_head();

		// If something doesnt work, do oom killing
		if (page == NULL || !swap_out(page)) {
			return 0;
		}
	}
	return 1;
}

/**
* Kills the most problematic process, thus freeing its memory.
* This function is called from page_reclaim() function if the swapping
* is not possible.
* Returns 1 (success) / 0 (fail)
*/
int oom_kill_process() {
    // Walk envs list and compute RSS for each process based on the stats 
    // stored there. Choose the process with the highest score.
	process_to_kill = -1;

    for (counter = 0; counter < NENV; counter++) {
    	// TODO fix - compute RSS score using env.used_pages
    	// I don't get the formula - what is RSS there?
    	current_rss = (envs[counter].num_alloc + envs[counter].num_swap + envs[counter].num_tables)
    	 / PAGE_SIZE + npages/1000;
    	cprintf("OOM process %llx, alloc %d, score %d\n", envs[counter].env_id, envs[counter].num_alloc, current_rss);
    	if (current_rss > highest_rss) {
    		highest_rss = current_rss;
    		process_to_kill = envs[counter].env_id;
    	}
    }

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
int page_reclaim(size_t num) {
    // First, try to swap num pages.
	// Num - because we might want to support small/huge pages.
    if (! swap_pages())
    	// If not successful, go for OOM killing.
    	return oom_kill_process();
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


