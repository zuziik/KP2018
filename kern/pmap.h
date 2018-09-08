/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_PMAP_H
#define JOS_KERN_PMAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/boot.h>
#include <inc/assert.h>
#include <inc/paging.h>

#include <inc/x86-64/memory.h>

struct env;

struct env;

struct env;

extern char bootstacktop[], bootstack[];

extern struct page_info *pages;
extern struct page_info *page_free_list;
extern size_t npages;
extern struct page_table *kern_pml4;

/*
 * This macro takes a kernel virtual address -- an address that points above
 * KERNBASE, where the machine's maximum 256MB of physical memory is mapped --
 * and returns the corresponding physical address.  It panics if you pass it a
 * non-kernel virtual address.
 */
#define PADDR(kva) _paddr(__FILE__, __LINE__, kva)

static inline physaddr_t _paddr(const char *file, int line, void *kva)
{
    if ((uintptr_t)kva < KERNEL_VMA)
        _panic(file, line, "PADDR called with invalid kva %08lx", kva);
    return (physaddr_t)kva - KERNEL_VMA;
}

/* This macro takes a physical address and returns the corresponding kernel
 * virtual address.  It panics if you pass an invalid physical address. */
#define KADDR(pa) _kaddr(__FILE__, __LINE__, pa)

static inline void *_kaddr(const char *file, int line, physaddr_t pa)
{
    if (PAGE_INDEX(pa) >= npages)
        _panic(file, line, "KADDR called with invalid pa %08lx", pa);
    return (void *)(pa + KERNEL_VMA);
}

enum {
    /* For page_alloc, zero the returned physical page. */
    ALLOC_ZERO = 1<<0,
    ALLOC_HUGE = 1<<1,
    ALLOC_PREMAPPED = 1<<2,
};

enum {
    /* For page_walk, tells whether to create normal page or huge page */
    CREATE_NORMAL = 1<<0,
    CREATE_HUGE   = 1<<1,
};

void mem_init(struct boot_info *boot_info);

void page_init(struct boot_info *boot_info);
struct page_info *page_alloc(int alloc_flags);
void page_free(struct page_info *pp);
int page_insert(struct page_table *pml4, struct page_info *pp, void *va, int perm);
void page_remove(struct page_table *pml4, void *va);
struct page_info *page_lookup(struct page_table *pml4, void *va, physaddr_t **entry);
void page_decref(struct page_info *pp);

void tlb_invalidate(struct page_table *pml4, void *va);

static inline physaddr_t page2pa(struct page_info *pp)
{
    return (pp - pages) << PAGE_TABLE_SHIFT;
}

static inline struct page_info *pa2page(physaddr_t pa)
{
    if (PAGE_INDEX(pa) >= npages)
        panic("pa2page called with invalid pa");

    return pages + PAGE_INDEX(pa);
}

static inline void *page2kva(struct page_info *pp)
{
    return KADDR(page2pa(pp));
}

#endif /* !JOS_KERN_PMAP_H */
