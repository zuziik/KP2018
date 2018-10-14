#include <kern/kthread.h>
#include <kern/ide.h>
#include <kern/pmap.h>

// LAB 7
size_t nfreepages;                       /* Total number of free pags in memory,
                                        updated with each alloc/free */
envid_t process_to_kill;
size_t highest_rss;
size_t current_rss;
int counter;

uint32_t nswapslots;					/* Total number of swap slots on disk (not updated on alloc/free) */
struct swap_slot *swap_slots;			/* Array of all swap_slots to keep track of them */
struct swap_slot *free_swap_slots;		/* Linked list of free swap slots */
struct swap_slot *swapped_pages;

// Struct(s) used to save faulting page faults
// extern struct page_fault *page_faults;
struct page_fault {
	physaddr_t page_addr;
	physaddr_t pte_addr;
};

// For each PAGE_SIZE/SECTSIZE sectors on disk (aligned)
struct swap_slot {
	uint8_t is_used;
	// physaddr_t *ptes; OR uintptr_t *vas; PROBLEM - what to remember if we want to be able to flush TLBs (VA and env?)
	// maybe we just need to give up and walk all the page tables online
	struct swap_slot *prev;
	struct swap_slot *next;
};

// TODO change the constant
#define FREEPAGE_THRESHOLD	512
#define SECTORS_PER_PAGE PAGE_SIZE/SECTSIZE

void swap_init();
void set_nfreepages(size_t num);
void inc_nfreepages(int huge);
void dec_nfreepages(int huge);
void inc_allocated_in_env(struct env *e);
void dec_allocated_in_env(struct env *e);
void inc_swapped_in_env(struct env *e);
void dec_swapped_in_env(struct env *e);
void inc_tables_in_env(struct env *e);
void dec_tables_in_env(struct env *e);

int available_freepages(size_t num);
void page_fault_queue_insert(uintptr_t fault_va);
int page_reclaim(size_t num);
int swap_pages();
int oom_kill_process();
void kthread_swap();


// Disk slot -> sector number on disk
static inline uint32_t slot2sector(struct swap_slot *slot)
{
    return (slot - swap_slots)/sizeof(struct swap_slot) * SECTORS_PER_PAGE;
}

// Sector number (aligned) -> disk slot
static inline struct swap_slot *sector2slot(uint32_t secno)
{
    return &swap_slots[secno/SECTSIZE];
}