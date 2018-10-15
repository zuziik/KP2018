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

// Struct(s) used to save faulting page faults
// extern struct page_fault *page_faults;
// struct page_fault {
// 	physaddr_t page_addr;
// 	physaddr_t pte_addr;
// };

// For each PAGE_SIZE/SECTSIZE sectors on disk (aligned)
struct swap_slot {
	uint8_t is_used;
	struct env_va_mapping *reverse_mapping; // all VAs (per env) that used to map the physical page
				      				   // that is now swapped out in the slot
	struct swap_slot *prev;
	struct swap_slot *next;
};

// Structure to keep track of VAs that have been swapped out
struct swapped_va {
    void *va;
    struct swap_slot *slot;
    struct swapped_va *next;
};

// Structure to keep track of VAs that map a specific physical page
// 2D struct - a list is stored per env
struct env_va_mapping {
	struct env *e;
	struct va_mapping *list;
	struct env_va_mapping *next;
};

// List of reverse mappings for a specific physical page and environment
struct va_mapping {
	void *va;
	int perm;
	struct va_mapping *next;	
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
    return (slot - swap_slots)/sizeof(struct swap_slot) * SECTORS_PER_PAGE;
}

// Sector number (aligned) -> disk slot
static inline struct swap_slot *sector2slot(uint32_t secno)
{
    return &swap_slots[secno/SECTSIZE];
}

// Environment -> index in envs array
static inline uint32_t env2i(struct env *e)
{
    return (e - envs)/sizeof(struct env);
}