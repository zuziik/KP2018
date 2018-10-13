#include <kern/swap.h>


void swap_alloc_update_counters(int huge) {
	if (huge)
		dec_freepages(SMALL_PAGES_IN_HUGE);
	else
		dec_freepages(1);

	// Locks?
	if (curenv != NULL) {
		// update curenv counter of used physical pages
		if (huge)
			curenv->used_pages -= SMALL_PAGES_IN_HUGE;
		else
			curenv->used_pages -= 1;
	}
}

void swap_free_update_counters(int huge) {
	if (huge)
		inc_freepages(SMALL_PAGES_IN_HUGE);
	else
		inc_freepages(1);

	// Locks?
	if (curenv != NULL) {
		// update curenv counter of used physical pages
		if (huge)
			curenv->used_pages += SMALL_PAGES_IN_HUGE;
		else
			curenv->used_pages += 1;
	}
}            

/**
* Initializes freepages counter. Called during boot.
*/
void set_freepages(size_t num) {
	// TODO locking?
	freepages = num;
}

/**
* Returns 1 if num free pages are available, using
* the freepages counter.
* Called just before allocation and from a kernel
* swap thread.
*/
int available_freepages(size_t num) {
	// TODO locking?
	return freepages >= num;
}

/**
* Increments the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages (we might consider removing the huge page
* support).
* This function is called from page_free().
*/
void inc_freepages(size_t num) {
	// TODO locking?
	freepages += num;
}

/**
* Decrements the counter of the total number of available
* free pages on the system by num.
* Num is 1 for small pages and SMALL_PAGES_IN_HUGE for
* huge pages (we might consider removing the huge page
* support).
* This function is called from page_alloc().
*/
void dec_freepages(size_t num) {
	// TODO locking?
	freepages -= num;
}


/**
* Inserts the faulting VA at the end of a faulting
* pages queue (first FIFO, later CLOCK).
* This function is called from the page fault handler,
* but only after a new page is allocated (i.e. not
* if the page fault was a permission issue).
* Expects fault_va to be aligned.
*/
void page_fault_queue_insert(uintptr_t fault_va) {
	// Double-linked list manipulation.
	// Question - will there be any restrictions on the list size?
	// We don't want to record ALL page faults that ever happened.
	// Question - should we also call this after COW?
	// Do we want to support huge pages as well? Now it's only for
	// small pages.
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
    if (! swap_pages(num))
    	// If not successful, go for OOM killing.
    	return oom_kill_process();
    return 1;
}

/**
* Swaps the first num pages from the LRU list (FIFO/CLOCK),
* either to the cache, or on the disk.
* Called both from direct reclaiming and periodic reclaiming function.
* Returns 1 (success) / 0 (fail)
*/
int swap_pages(size_t num) {
	// !!! Are we going to distinguish between small/huge pages?
	// Or are we going to always swap-out 512 pages, so that we
	// always have at least one huge page ready?
	return 0;
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

    for (oom_i = 0; oom_i < NENV; oom_i++) {
    	// TODO fix - compute RSS score using env.used_pages
    	// I don't get the formula - what is RSS there?
    	current_rss = envs[oom_i].used_pages;
    	if (current_rss > highest_rss) {
    		highest_rss = current_rss;
    		process_to_kill = envs[oom_i].env_id;
    	}
    }

    if (process_to_kill == -1) {
    	panic("OOM killer - No processes to kill!");
    }

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
* KERNEL THREAD
* Periodically checks whether the number of free pages is above a given threshold.
* If not, swaps pages .
* It uses global variables without locking because only one instance of this
* kernel thread is running at a time.
*/
void kthread_swap() {
	// TODO fix the constants. We need something like THRESHOLD instead of 555
	// and them compute the number using another threshold instead of 333.
    while (1) {
        if (! available_freepages(FREEPAGE_THRESHOLD))
        	swap_pages(333);
        kthread_interrupt();
    }
}