#include <kern/swap.h>
#include <kern/vma.h>

struct page_info *page_fault_head = NULL;

/*
 * Initialization of swapping functionality. Prepares control
 * datastructure to keep track on changes on the disk.
 */
void swap_init() {
    uintptr_t vi;
	uintptr_t va_start;
    uintptr_t va_end;
    struct page_info *p;
    uint32_t size;
    int i;

    //*********************************************************

	// Allocating memory for swap slots datastructure
	// (information about free/used sectors on disk)

	nswapslots = ide_num_sectors() / (PAGE_SIZE/SECTSIZE);
	cprintf("DEBUG: [SWAP INIT] We can fit %d pages on swap disk\n", nswapslots);

	size = sizeof(struct swap_slot)*nswapslots;

    p = NULL;

    va_start = (uintptr_t) SWAPSLOTS;
    va_end = ROUNDUP(va_start + size, PAGE_SIZE);


    for (vi = va_start; vi < va_end; vi += PAGE_SIZE) {
        if (!(p = page_alloc(ALLOC_ZERO)))
            panic("Couldn't allocate memory for swap disk structure");
        // TODO PAGE_NO_EXEC? (I removed PAGE_USER)
        page_insert(kern_pml4, p, (void *)vi, PAGE_WRITE);
    }

	swap_slots = (struct swap_slot *) va_start;
	cprintf("DEBUG: swap_slot size: %llx\n", sizeof(struct swap_slot));

	// Initializing the data structure
	for (i = 0; i < nswapslots; i++) {
        swap_slots[i].next = (i == nswapslots - 1) ? NULL : &swap_slots[i+1];
        swap_slots[i].prev = (i == 0) ? NULL : &swap_slots[i-1];
	}

	// Marking all swap slots as free, thus putting them in the free slots list
	free_swap_slots = swap_slots;

	//*********************************************************

	// Allocating memory for a pool of allocated swapped structures
	// (to be used to keep track of swapped out pages)

	va_start = (uintptr_t) POOL_SWAPPED;

	p = page_alloc(ALLOC_ZERO);
	if (p == NULL)
		panic("Couldn't allocate memory for swapped pool in swap_init()");

	page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

	npages_swapped = 1;
	nswapped = PAGE_SIZE / sizeof(struct swapped);
	cprintf("DEBUG: [SWAP] number of swapped structures: %d\n", nswapped);

	swapped = (struct swapped *) va_start;

	// Initializing the data structure (other fields are left 0)
	for (i = 0; i < nswapped - 1; i++) {
        swapped[i].next = &swapped[i+1];
	}

	// Marking all structs as free, thus putting them in the free list
	free_swapped = swapped;

	//*********************************************************

	// Allocating memory for a pool of allocated mapping structures
	// (to be used to keep track of reverse mappings PA->VA)

	va_start = (uintptr_t) POOL_MAPPING;

	p = page_alloc(ALLOC_ZERO);
	if (p == NULL)
		panic("Couldn't allocate memory for mapping pool in swap_init()");

	page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

	npages_mapping = 1;
	nmappings = PAGE_SIZE / sizeof(struct mapping);
	cprintf("DEBUG: [SWAP] number of mapping structures: %d\n", nmappings);

	mappings = (struct mapping *) va_start;

	// Initializing the data structure (other fields are left 0)
	for (i = 0; i < nmappings - 1; i++) {
        mappings[i].next = &mappings[i+1];
	}

	// Marking all structs as free, thus putting them in the free list
	free_mappings = mappings;

	//*********************************************************

	// Allocating memory for a pool of allocated env_mapping structures
	// (to be used to keep track of reverse mappings ENV,PA->va)

	va_start = (uintptr_t) POOL_ENV_MAPPING;

	p = page_alloc(ALLOC_ZERO);
	if (p == NULL)
		panic("Couldn't allocate memory for env_mapping pool in swap_init()");

	page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

	npages_env_mapping = 1;
	nenvmappings = PAGE_SIZE / sizeof(struct env_mapping);
	cprintf("DEBUG: [SWAP] number of env_mapping structures: %d\n", nenvmappings);

	env_mappings = (struct env_mapping *) va_start;

	// Initializing the data structure (other fields are left 0)
	for (i = 0; i < nenvmappings - 1; i++) {
        env_mappings[i].next = &env_mappings[i+1];
	}

	// Marking all structs as free, thus putting them in the free list
	free_env_mappings = env_mappings;
}

//-----------------------------------------------------------------------------
// Pool for struct swap_slot
//-----------------------------------------------------------------------------

/*
 * Removes head from the free_swap_slots list
 * Returns swap slot or NULL
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

/*
 * Marks the swap space slot as free, i.e. adds it to the free list
 */
void free_swap_slot(struct swap_slot *slot) {
	slot->next = free_swap_slots;
	slot->prev = NULL;
	free_swap_slots->prev = slot;
	free_swap_slots = slot;
}

//-----------------------------------------------------------------------------
// Pool for struct swapped
//-----------------------------------------------------------------------------

/*
 * Removes head from the free_swapped list, returns swapped struct.
 * If there are no more preallocated structures,
 * allocates another page and of the structs and adds them into the pool.
 * Panics if we can't allocate any new struct
 */
struct swapped *alloc_swapped_struct() {
	int i;
	uint64_t old_nswapped;
	uintptr_t va_start;

	if (free_swapped == NULL) {
		if (npages_swapped == MAX_POOL_SIZE) {
			panic("Can't allocate any more swapped structs - pool reached max size");
		}
		struct page_info *p = page_alloc(ALLOC_ZERO);
		if (p == NULL) {
			panic("Can't allocate any more swapped structs - no pages available");
		}

		va_start = (uintptr_t) POOL_SWAPPED + npages_swapped*PAGE_SIZE;
		page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

		npages_swapped++;

		old_nswapped = nswapped;
		nswapped = (PAGE_SIZE*npages_swapped)/sizeof(struct swapped);

		for (i = old_nswapped; i < nswapped - 1; i++) {
			swapped[i].next = &swapped[i+1];
		}

		free_swapped = &swapped[old_nswapped];
	}

	struct swapped *swapped = free_swapped;
	free_swapped = free_swapped->next;
	return swapped;
}

/*
 * Marks the swapped struct as free, i.e. adds it to the free list
 */
void free_swapped_struct(struct swapped *swapped) {
	swapped->va = NULL;
	swapped->slot = NULL;
	swapped->next = free_swapped;
	free_swapped = swapped;
}

//-----------------------------------------------------------------------------
// Pool for struct mapping
//-----------------------------------------------------------------------------

/*
 * Removes head from the free_mappings list, returns mapping struct.
 * If there are no more preallocated structures,
 * allocates another page and of the structs and adds them into the pool.
 * Panics if we can't allocate any new struct
 */
struct mapping *alloc_mapping_struct() {
	int i;
	uint64_t old_nmappings;
	uintptr_t va_start;

	if (free_mappings == NULL) {
		if (npages_mapping == MAX_POOL_SIZE) {
			panic("Can't allocate any more mapping structs - pool reached max size");
		}
		struct page_info *p = page_alloc(ALLOC_ZERO);
		if (p == NULL) {
			panic("Can't allocate any more mapping structs - no pages available");
		}

		va_start = (uintptr_t) POOL_MAPPING + npages_mapping*PAGE_SIZE;
		page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

		npages_mapping++;

		old_nmappings = nmappings;
		nmappings = (PAGE_SIZE*npages_mapping)/sizeof(struct mapping);

		for (i = old_nmappings; i < nmappings - 1; i++) {
			mappings[i].next = &mappings[i+1];
		}

		free_mappings = &mappings[old_nmappings];
	}

	struct mapping *mapping = free_mappings;
	free_mappings = free_mappings->next;
	return mapping;
}

/*
 * Marks the mapping struct as free, i.e. adds it to the free list
 */
void free_mapping_struct(struct mapping *mapping) {
	mapping->va = NULL;
	mapping->perm = 0;
	mapping->next = free_mappings;
	free_mappings = mapping;
}

//-----------------------------------------------------------------------------
// Pool for struct env_mapping
//-----------------------------------------------------------------------------

/*
 * Removes head from the free_env_mappings list, returns env_mapping struct.
 * If there are no more preallocated structures,
 * allocates another page and of the structs and adds them into the pool.
 * Panics if we can't allocate any new struct (reached the max pool size)
 */
struct env_mapping *alloc_env_mapping_struct() {
	int i;
	uint64_t old_nenvmappings;
	uintptr_t va_start;

	if (free_env_mappings == NULL) {
		if (npages_env_mapping == MAX_POOL_SIZE) {
			panic("Can't allocate any more env_mapping structs - pool reached max size");
		}
		struct page_info *p = page_alloc(ALLOC_ZERO);
		if (p == NULL) {
			panic("Can't allocate any more env_mapping structs - no pages available");
		}

		va_start = (uintptr_t) POOL_ENV_MAPPING + npages_env_mapping*PAGE_SIZE;
		page_insert(kern_pml4, p, (void *) va_start, PAGE_WRITE);

		npages_env_mapping++;

		old_nenvmappings = nenvmappings;
		nenvmappings = (PAGE_SIZE*npages_env_mapping)/sizeof(struct env_mapping);

		for (i = old_nenvmappings; i < nenvmappings - 1; i++) {
			env_mappings[i].next = &env_mappings[i+1];
		}

		free_env_mappings = &env_mappings[old_nenvmappings];
	}

	struct env_mapping *env_mapping = free_env_mappings;
	free_env_mappings = free_env_mappings->next;
	return env_mapping;
}

/*
 * Marks the env_mapping struct as free, i.e. adds it to the free list
 */
void free_env_mapping_struct(struct env_mapping *env_mapping) {
	env_mapping->e = NULL;
	env_mapping->list = NULL;
	env_mapping->next = free_env_mappings;
	free_env_mappings = env_mapping;
}

//-----------------------------------------------------------------------------
// Keeping track of reverse mappings (physical page -> env & virtual page)
//-----------------------------------------------------------------------------

/*
 * Adds a reverse mapping to a physical page
 */
void add_reverse_mapping(struct env *e, void *va, struct page_info *page, int perm) {
	struct mapping *new_mapping = alloc_mapping_struct();	

	struct env_mapping *tmp_env_mapping;

	new_mapping->va = va;
	new_mapping->perm = perm;

	tmp_env_mapping = page->reverse_mapping;

	while ((tmp_env_mapping != NULL) && (tmp_env_mapping->e != e))
		tmp_env_mapping = tmp_env_mapping->next;

	// Nothing is mapped for the env yet
	if (tmp_env_mapping == NULL) {
		tmp_env_mapping = alloc_env_mapping_struct();
		tmp_env_mapping->e = e;
		tmp_env_mapping->next = page->reverse_mapping;
		page->reverse_mapping = tmp_env_mapping;
	}

	new_mapping->next = tmp_env_mapping->list;
	tmp_env_mapping->list = new_mapping;
}

/*
 * Removes a reverse mapping from a physical page
 */
void remove_reverse_mapping(struct env *e, void *va, struct page_info *page) {

	struct mapping *curr_mapping;	
	struct mapping *prev_mapping;	
	struct env_mapping *curr_env_mapping = page->reverse_mapping;
	struct env_mapping *prev_env_mapping = NULL;

	while ((curr_env_mapping != NULL) && (curr_env_mapping->e != e)) {
		prev_env_mapping = curr_env_mapping;
		curr_env_mapping = curr_env_mapping->next;
	}

	// Mapping for env is not there, nothing to remove, return
	if (curr_env_mapping == NULL)
		return;

	curr_mapping = curr_env_mapping->list;
	prev_mapping = NULL;

	while ((curr_mapping != NULL) && (curr_mapping->va != va)) {
		prev_mapping = curr_mapping;
		curr_mapping = curr_mapping->next;
	}

	// Mapping for this env and va is not there, nothing to remove, return
	if (curr_mapping == NULL)
		return;

	// VA is the first in the list
	if (prev_mapping == NULL)
		curr_env_mapping->list = curr_mapping->next;
	else
		prev_mapping->next = curr_mapping->next;

	free_mapping_struct(curr_mapping);

	// All mappings of the environment were freed, remove the list for the environment
	if (curr_env_mapping->list == NULL) {
		if (prev_env_mapping == NULL)
			page->reverse_mapping = curr_env_mapping->next;
		else
			prev_env_mapping->next = curr_env_mapping->next;
		free_env_mapping_struct(curr_env_mapping);
	}
}

/*
 * Removes reverse mappings of this environment from all pages
 * Called from env_free()
 */
void env_remove_reverse_mappings(struct env *e) {
	int i;

	struct mapping *curr_mapping;
	struct mapping *next_mapping;
	struct env_mapping *curr_env_mapping;
	struct env_mapping *prev_env_mapping;

	// Remove mappings for each page
	for (i = 0; i < npages; i++) {
		if (pages[i].is_available == PAGE_FREE) {
			continue;
		}

		curr_env_mapping = pages[i].reverse_mapping;
		prev_env_mapping = NULL;

		// Find list of mappings for the specified environment
		while ((curr_env_mapping != NULL) && (curr_env_mapping->e != e)) {
			prev_env_mapping = curr_env_mapping;
			curr_env_mapping = curr_env_mapping->next;
		}
		// Mapping for env is not there, nothing to remove, continue with next page
		if (curr_env_mapping == NULL) {
			continue;
		}

		curr_mapping = curr_env_mapping->list;

		while (curr_mapping != NULL) {
			next_mapping = curr_mapping->next;
			free_mapping_struct(curr_mapping);
			curr_mapping = next_mapping;
		}

		// Remove env list from the page mappings
		if (prev_env_mapping == NULL)
			pages[i].reverse_mapping = curr_env_mapping->next;
		else
			prev_env_mapping->next = curr_env_mapping->next;
		    free_env_mapping_struct(curr_env_mapping);
	}
}

//-----------------------------------------------------------------------------
// Keeping track of swapped pages (per env, per VMA)
//-----------------------------------------------------------------------------

/*
 * Searches swapped pages list of the given VMA and looks for the given VA.
 * If found, returns pointer to the swap slot where the page is swapped out.
 */
struct swap_slot *vma_lookup_swapped_page(struct vma *vma, void *va) {
	struct swapped *swapped = vma->swapped_pages;
	while ((swapped != NULL) && (swapped->va != va)) {
		swapped = swapped->next;
	}
	if (swapped == NULL) {
		return NULL;
	}
	return swapped->slot;
}
 
/*
 * Adds the VA and swap slot to the corresponding VMA list of swapped out pages.
 */     
void vma_add_swapped_page(struct env *e, void *va, struct swap_slot *slot) {
	struct vma *vma = vma_lookup(e, va);
	if (vma == NULL)
		return;

	struct swapped *swapped = alloc_swapped_struct();
	swapped->va = va;
	swapped->slot = slot;
	swapped->next = vma->swapped_pages;

	vma->swapped_pages = swapped;
}

/*
 * Removes the VA from the corresponding VMA list of swapped out pages.
 */
void vma_remove_swapped_page(struct env *e, void *va) {
	struct vma *vma = vma_lookup(e, va);
	if (vma == NULL)
		return;

	struct swapped *curr = vma->swapped_pages;
	struct swapped *prev = vma->swapped_pages;

	if (curr->va == va) {
		vma->swapped_pages = curr->next;
		free_swapped_struct(curr);
		return;
	}

	curr = curr->next;

	while (curr != NULL) {
		if (curr->va == va) {
			prev->next = curr->next;
			free_swapped_struct(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}

	panic("swapped_va not in vma list\n");
}

//-----------------------------------------------------------------------------
// Keeping track of total number of free pages in memory
//-----------------------------------------------------------------------------

/*
 * Initializes freepages counter. Called during boot.
 */
void set_nfreepages(size_t num) {
	lock_nfreepages();
	nfreepages = num;
	nfreepages = 2500;
	unlock_nfreepages();
}

/*
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

/*
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

/*
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

//-----------------------------------------------------------------------------
// Keeping track of faulting pages -- LRU (CLOCK queue)
//-----------------------------------------------------------------------------

/*
 * CRemoves the page from the LRU queue if it's
 * in the list.
 */
void page_fault_remove(struct page_info *page) {

	// Not in the list
	if (page->fault_next == NULL) {
		return;
	}
	// It's the only element in the list
	else if (page == page->fault_next) {
		page_fault_head = NULL;
	}
	// At least two elements in the list
	else {
		page->fault_prev->fault_next = page->fault_next;
		page->fault_next->fault_prev = page->fault_prev;
		page_fault_head = page->fault_next;
	}
}

/*
 * Inserts the faulting VA at the end of a faulting
 * pages queue (circular - CLOCK).
 * This function is called from the page fault handler,
 * but only after a new page is allocated (i.e. not
 * if the page fault was a permission issue).
 * Expects fault_va to be aligned.
 */
void page_fault_queue_insert(uintptr_t fault_va) {
	int lock = swap_lock_pagealloc();

	struct vma *vma;
	struct page_info *page;
	physaddr_t *pt_entry = NULL;
	page = page_lookup(curenv->env_pml4, (void *) fault_va, &pt_entry);

	// If already in the list, move it to the back
	if (page_fault_head != NULL && page->fault_next != NULL && page->fault_next != page_fault_head)
		page_fault_remove(page);

	// Empty list
	if (page_fault_head == NULL) {
		page->fault_prev = page;
		page->fault_next = page;
		page_fault_head = page;
	}
	// Append to the back
	else  {
		page->fault_prev = page_fault_head->fault_prev;
		page->fault_next = page_fault_head;
		page_fault_head->fault_prev->fault_next = page;
		page_fault_head->fault_prev = page;
	}

	swap_unlock_pagealloc(lock);
}

/*
 * Clears PAGE_ACCESSED bit on all PTEs mapping the page.
 * Returns 1 if the bit was set at least at one mapping,
 * 0 otherwise.
 */
int clear_accessed_bit(struct page_info *page) {
	struct env_mapping *env_mapping;
	struct mapping *mapping;
	physaddr_t *addr;

	int accessed = 0;

	env_mapping = page->reverse_mapping;
	while (env_mapping != NULL) {
		mapping = env_mapping->list;
		while (mapping != NULL) {
			addr = page_walk(env_mapping->e->env_pml4, mapping->va, 0);
			if (*addr & PAGE_ACCESSED) {
				accessed = 1;
				*addr &= ~PAGE_ACCESSED;
			}
			mapping = mapping->next;
		}
		env_mapping = env_mapping->next;
	}

	return accessed;
}

/*
 * Returns a page from LRU queue to be swapped out, or NULL
 * if the queue is empty.
 * Generally, it removes the head of the queue, but it has a
 * "second chance" option - if the head has been accessed
 * since the page fault, its PAGE_ACCESS bit is cleared and
 * it is placed at the end of the list. We repeat this until
 * we find a page to swap out.
 */
struct page_info *page_fault_pop_head() {
	struct page_info *page = NULL;

	// Page fault list is empty
	if (page_fault_head == NULL) {
		return NULL;
	}

	// Only 1 element in the list
	else if (page_fault_head->fault_next == NULL) {
		page = page_fault_head;
		page->fault_prev = NULL;
		page->fault_next = NULL;
		page_fault_head = NULL;
		return page;
	}

	// CLOCK: search for head with no second chance
	while (1) {
		page = page_fault_head;

		if (clear_accessed_bit(page)) {
			page_fault_head = page_fault_head->fault_next;
		}
		else {
			page_fault_remove(page);
			return page;
		}
	}
}


//-----------------------------------------------------------------------------
// Page reclaiming functions - direct and periodic swapping
//-----------------------------------------------------------------------------

/*
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
	struct env_mapping *env_mapping;
	struct mapping *mapping;
	struct cpuinfo *c;

	int kern = env_lock_env();
    assert_lock_env();

	lock_swapslot();
	slot = alloc_swap_slot();

	// No swap space available, return fail
	if (slot == NULL) {
		unlock_swapslot();
		env_unlock_env(kern);
		return 0;
	}
	
	ide_start_write(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		while (!ide_is_ready());
		ide_write_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);
	}

	// 2. Remove VMA list from page_info and map it to swap structure
	slot->reverse_mapping = p->reverse_mapping;
	p->reverse_mapping = NULL;

	// 3. Zero-out all entries pointing to this physical page and
	// decrease reference count on the page
	// 4. Add the slot to every VMA swapped_pages list
	// 5. The page should be freed automatically since we remove all references
	env_mapping = slot->reverse_mapping;
	
	while (env_mapping != NULL) {
		mapping = env_mapping->list;
		while (mapping != NULL) {
			// LAB 7 - for 
			page_remove(env_mapping->e->env_pml4, mapping->va);
			vma_add_swapped_page(env_mapping->e, mapping->va, slot);
			mapping = mapping->next;
		}
		// "Flush TLB" for the environment if it's not running at the our CPU
		// (sends IPI that calls a scheduler which should flush TLB automatically)
		if ((env_mapping->e->env_status == ENV_RUNNING) && (env_mapping->e != curenv)) {
			c = &cpus[process_to_kill->env_cpunum];
    		lapic_ipi_target(c->cpu_id, IRQ_SUSPEND);	
		}
		env_mapping = env_mapping->next;
	}

	unlock_swapslot();
	env_unlock_env(kern);
	return 1;
}

/*
 * Allocates a new physical page and copies it back from disk.
 * + There will be another function that calls this one, which
 * first walks the swap_slots array to search for the slot which
 * contains the swapped out VA+env / PTE.
 * Returns 1 (success) / 0 (fail)
*/
int swap_in(struct swap_slot *slot) {
	cprintf("[SWAP_IN] start\n");
	int i;
	struct env_mapping *env_mapping;
	struct mapping *mapping;

	// 1. Allocate a page
	struct page_info *p = page_alloc(0);
	if (p == NULL)
		return 0;

	// 2. Copy data back from the disk
	lock_swapslot();
	ide_start_read(slot2sector(slot), SECTORS_PER_PAGE);
	for (i = 0; i < SECTORS_PER_PAGE; i++) {
		while (!ide_is_ready());
		ide_read_sector((char *) KADDR(page2pa(p)) + i*SECTSIZE);
	}

	// 3. Free the swap space
	free_swap_slot(slot);


	// 4. Walk the saved reverse mappings and map the page everywhere
	// it was mapped before + increase the refcount with each mapping
	// 5. Remove the slot from swapped_pages list of each VMA
	env_mapping = slot->reverse_mapping;
	while (env_mapping != NULL) {
		mapping = env_mapping->list;
		while (mapping != NULL) {
			page_insert(env_mapping->e->env_pml4, p, mapping->va, mapping->perm);
			vma_remove_swapped_page(env_mapping->e, mapping->va);
			mapping = mapping->next;
		}
		env_mapping = env_mapping->next;
	}

	// 6. Correctly set the reverse mappings to the new page
	p->reverse_mapping = slot->reverse_mapping;
	slot->reverse_mapping = NULL;

	unlock_swapslot();
	return 1;
}

/*
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
	unlock_nfreepages();

	int i;
	struct page_info *page;

	// Pop to_free times and swap out
	int lock = swap_lock_pagealloc();
	for (i = 0; i < to_free; i++) {
		cprintf("DEBUG: START_i: %d / %d\n", i, to_free);
		page = page_fault_pop_head();

		// If something doesnt work, do oom killing
		if (page == NULL || !swap_out(page)) {
			cprintf("START_SWAP_PAGES_CRASH\n");
			swap_unlock_pagealloc(lock);
			return 0;
		}
	}
	swap_unlock_pagealloc(lock);
	return 1;
}

/*
 * On-demand page reclaiming called if we are out-of memory (in page_alloc).
 * We could also be out-of-memory in boot_alloc but that would be a worse
 * problem and we don't want to swap kernel pages, so we don't call this
 * function from there.
 * Returns 1 (success) / 0 (fail)
 */
int page_reclaim() {
	// Remove unnecessary locks
	cprintf("DEBUG: [PAGE_RECLAIM] start\n");

	int lock = 0;
	int res = 0;
	if (holding(&pagealloc_lock)) {
		lock++;
		unlock_pagealloc();
	}

	cprintf("START_SWAP_PAGES\n");
    // First, try to swap num pages.
    if (! swap_pages()) {
    	cprintf("START_OOM_KILLING\n");
    	// If not successful, go for OOM killing.
    	res = oom_kill_process();
    	if (lock) {
	    	lock_pagealloc();
	    }
	    return res;
    }

    if (lock) {
    	lock_pagealloc();
    }
    return 1;
}

/*
 * KERNEL THREAD
 * Periodically checks whether the number of free pages is above a given threshold.
 * If not, swaps enough pages to have FREEPAGE_THRESHOLD free pages.
 * Only one instance of this
 * kernel thread is running at a time.
 */
void kthread_swap() {
    while (1) {
    	cprintf("[KTHREAD_SWAP] start\n");
        if (! available_freepages(FREEPAGE_THRESHOLD))
        	swap_pages();
        kthread_interrupt();
    }
}

//-----------------------------------------------------------------------------
// OOM killer functions
//-----------------------------------------------------------------------------

/*
 * Returns the number of swapped out pages for the specified envuronment.
 * Used by the oom_kill_process() function.
 * OLD version (tested but not correct - can count 1 swapped page several times)
 */
// uint32_t count_swapped_pages(struct env* e) {
// 	struct vma *vma;
// 	struct swapped *swapped_page;
// 	uint32_t num_swap = 0;

//    	vma = e->vma;

// 	while (vma != NULL) {
// 		swapped_page = vma->swapped_pages;
// 		while (swapped_page != NULL) {
// 			num_swap++;
// 			swapped_page = swapped_page->next;
// 		}
// 		vma = vma->next;
//     }

//     return num_swap;
// }

/*
 * Returns the number of swapped out pages for the specified envuronment.
 * Used by the oom_kill_process() function.
 * NEW version - 1 swapped page is counter once
 */
uint32_t count_swapped_pages(struct env* e) {
	int i;
	uint32_t num_swap = 0;
	struct env_mapping *env_mapping;

	for (i = 0; i < nswapslots; i++) {
		env_mapping = swap_slots[i].reverse_mapping;
		while ((env_mapping != NULL) && (env_mapping->e != e)) {
			env_mapping = env_mapping->next;
		}
		// This page was used by the environment (before swapping)
		// -> increment the counter
		if (env_mapping != NULL)
			num_swap++;
	}

    return num_swap;
}

/*
 * Returns the number of allocated pages for the specified envuronment.
 * (only mapped pages, not page tables)
 * Used by the oom_kill_process() function.
 */
uint32_t count_allocated_pages(struct env* e) {
	struct env_mapping *env_mapping;
	uint32_t num_alloc = 0;
	int i;

	for (i = 0; i < npages; i++) {
		env_mapping = pages[i].reverse_mapping;
		while ((env_mapping != NULL) && (env_mapping->e != e)) {
			env_mapping = env_mapping->next;
		}
		// This page is used by the environment - increment the counter
		if (env_mapping != NULL)
			num_alloc++;
	}

	return num_alloc;
}

/*
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

/*
 * Kills the most problematic process, thus freeing its memory.
 * This function is called from page_reclaim() function if the swapping
 * is not possible.
 * Returns 1 (success) / 0 (fail)
 */
int oom_kill_process() {
	process_to_kill = NULL;	

    // Walk envs list and compute RSS for each process based on the stats 
    // stored there. Choose the process with the highest score.
	lock_env();
    for (counter = 0; counter < NENV; counter++) {
    	if (envs[counter].env_status == ENV_FREE)
    		continue;
    	current_rss = (count_allocated_pages(&envs[counter]) 
    		+ count_swapped_pages(&envs[counter]) 
    		+ count_table_pages(&envs[counter])
    		) / PAGE_SIZE + npages/1000;
    	cprintf("DEBUG: OOM process %llx, alloc %d, swap %d, table %d, score %d\n", envs[counter].env_id, count_allocated_pages(&envs[counter]), count_swapped_pages(&envs[counter]), count_table_pages(&envs[counter]), current_rss);
    	if (current_rss > highest_rss) {
    		highest_rss = current_rss;
    		process_to_kill = &envs[counter];
    	}
    }

    if (process_to_kill == NULL) {
    	unlock_env();
    	return 0;
    }

    // Kill the process
    cprintf("OOM wants to kill %llx\n", process_to_kill->env_id);

    // Not running - can destroy immediately. It is another process so we don't
    // want to go back to the scheduler but serve the page_alloc that triggered
    // the OOM killer.
    if (process_to_kill->env_status != ENV_RUNNING) {
    	env_destroy(process_to_kill);
    }
    // If it's on this processor, destroy the current environment and go back to
    // the scheduler - no need to serve the page_alloc for an environment we just
    // destroyed
    else if (process_to_kill == curenv) {
    	env_destroy(process_to_kill);
    }
    // It's on another core, send IPI to destroy whatever is running there. We
    // we will return to the page_alloc().
    else {
    	struct cpuinfo *c = &cpus[process_to_kill->env_cpunum];
    	lapic_ipi_target(c->cpu_id, IRQ_KILL);
    }

    unlock_env();
    return 1;
}