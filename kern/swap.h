#include <kern/kthread.h>

// LAB 7
size_t freepages;                       /* Total number of free pags in memory,
                                        updated with each alloc/free */
envid_t process_to_kill;
size_t highest_rss;
size_t current_rss;
int oom_i;

// Struct(s) used to save faulting page faults
// extern struct page_fault *page_faults;

struct page_fault {
	physaddr_t page_addr;
	physaddr_t pte_addr;
};

// TODO change the constant
#define FREEPAGE_THRESHOLD	256

void swap_alloc_update_counters(int huge);
void swap_free_update_counters(int huge);
void set_freepages(size_t num);
void inc_freepages(size_t num);
void dec_freepages(size_t num);
int available_freepages(size_t num);
void page_fault_queue_insert(uintptr_t fault_va);
int page_reclaim(size_t num);
int swap_pages(size_t num);
int oom_kill_process();
void kthread_swap();