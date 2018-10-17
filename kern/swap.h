#include <kern/kthread.h>
#include <kern/ide.h>
#include <kern/pmap.h>

// LAB 7
size_t nfreepages;                       /* Total number of free pags in memory,
                                        updated with each alloc/free */
struct env *process_to_kill;
size_t highest_rss;
size_t current_rss;
int counter;

uint32_t nswapslots;					/* Total number of swap slots on disk (not updated on alloc/free) */
struct swap_slot *swap_slots;			/* Array of all swap_slots to keep track of them */
struct swap_slot *free_swap_slots;		/* Linked list of free swap slots */

uint32_t nswapped;						/* Total number of currently allocated swapped structures,
										can change dynamically on-demand */
uint64_t npages_swapped;				/* Current number of pages allocated for the swapped structures */
struct swapped *swapped;				/* Array of all allocated swapped structures */
struct swapped *free_swapped;			/* Linked list of allocated but unused swapped structures*/

uint32_t nenvmappings;					/* Total number of currently allocated env_mapping structures,
										can change dynamically on-demand */
uint64_t npages_env_mapping;			/* Current number of pages allocated for the env_mapping structures */
struct env_mapping *env_mappings;		/* Array of all allocated env_mapping structures */
struct env_mapping *free_env_mappings;	/* Linked list of allocated but unused env_mapping structures*/

uint32_t nmappings;						/* Total number of currently allocated mapping structures,
										can change dynamically on-demand */
uint64_t npages_mapping;				/* Current number of pages allocated for the mapping structures */
struct mapping *mappings;				/* Array of all allocated mapping structures */
struct mapping *free_mappings;			/* Linked list of allocated but unused mapping structures*/

// For each PAGE_SIZE/SECTSIZE sectors on disk (aligned)
struct swap_slot {
	struct env_mapping *reverse_mapping; // all VAs (per env) that used to map the physical page
				      				   // that is now swapped out in the slot
	struct swap_slot *prev;
	struct swap_slot *next;
};

// Structure to keep track of VAs that have been swapped out
struct swapped {
    void *va;
    struct swap_slot *slot;
    struct swapped *next;
};

// Structure to keep track of VAs that map a specific physical page
// 2D struct - a list is stored per env
struct env_mapping {
	struct env *e;
	struct mapping *list;
	struct env_mapping *next;
};

// List of reverse mappings for a specific physical page and environment
struct mapping {
	void *va;
	int perm;
	struct mapping *next;	
};


#define FREEPAGE_THRESHOLD	512				// When does memory pressure start
#define FREEPAGE_OVERTHRESHOLD 512			// On memory pressure: free x beneath threshold
#define SECTORS_PER_PAGE PAGE_SIZE/SECTSIZE

void swap_init();

int swap_in(struct swap_slot *slot);
int swap_out(struct page_info *p);

void set_nfreepages(size_t num);
void inc_nfreepages(int huge);
void dec_nfreepages(int huge);

int available_freepages(size_t num);
void page_fault_remove(struct page_info *page);
void page_fault_queue_insert(uintptr_t fault_va);
int page_reclaim();
int swap_pages();
int oom_kill_process();
void kthread_swap();

void vma_remove_swapped_page(struct env *e, void *va);
void vma_add_swapped_page(struct env *e, void *va, struct swap_slot *slot);
struct swap_slot *vma_lookup_swapped_page(struct vma *vma, void *va);

void add_reverse_mapping(struct env *e, void *va, struct page_info *page, int perm);
void remove_reverse_mapping(struct env *e, void *va, struct page_info *page);
void env_remove_reverse_mappings(struct env *e);

// Disk slot -> sector number on disk
static inline uint32_t slot2sector(struct swap_slot *slot)
{
    return (slot - swap_slots) * SECTORS_PER_PAGE;
}

// Sector number (aligned) -> disk slot
static inline struct swap_slot *sector2slot(uint32_t secno)
{
	// ZUZANA: this should be SECTORS_PER_PAGE instead of 8 but if I change it, the result of the division is always 0 :O
    return &swap_slots[secno/8];
}

// Environment -> index in envs array
static inline uint32_t env2i(struct env *e)
{
    return (e - envs)/sizeof(struct env);
}