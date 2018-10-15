#include <kern/vma.h>

/**
* Removes the specified VMA from the VMAs list
* of the given environment, i.e. sets the type
* as VMA_UNUSED and moves it at the end of the list.
*/
void vma_make_unused(struct env *env, struct vma *vma) {

    struct vma *last_vma;
    // First remove the entry and then put it at the end

    // VMA is first in the list
    if (vma->prev == NULL) {
        env->vma = vma->next;
        if (env->vma != NULL) {
            (vma->next)->prev = NULL;
        }
    }
    
    // VMA is not the first entry
    else {
        (vma->prev)->next = vma->next;
        if (vma->next != NULL) {
            (vma->next)->prev = vma->prev;
            }
    }

    // Now reset all values and append at end
    last_vma = vma_get_last(curenv->vma);
    vma->va = NULL;
    vma->next = NULL;
    vma->type = VMA_UNUSED;
    vma->len = 0;
    vma->perm = 0;
    vma->mem_va = NULL;
    vma->file_va = NULL;
    vma->mem_size = 0;
    vma->file_size = 0;
    vma->swapped_pages = NULL;

    vma->prev = last_vma;
    last_vma->next = vma;
}

/**
* Given the environment and a virtual address,
* returns the VMA that contains the virtual address
* or NULL if none contains it.
*/
struct vma *vma_lookup(struct env *env, void *va) {
    struct vma *vma = env->vma;
    uintptr_t virt_addr = (uintptr_t) va;

    // Search linked list of vma's for correct vma
    while (vma != NULL) {
        // Vma is free so not found
        if (vma->type == VMA_UNUSED) {
            return NULL;
        }
        // Check if this vma is the correct one
        else if (virt_addr >= (uintptr_t) vma->va &&
            virt_addr < (uintptr_t)vma->va + vma->len) {
            break;
        }

        vma = vma->next;
    }

    return vma;
}

/**
* Inserts a VMA for the specified VA range <va, va+len)
* into the VMAs list of the specified environment.
*
* Possible types:  VMA_ANON / VMA_BINARY (VMA_UNUSED means free)
* Fields binary_start and binary_size are only used with
* binary type and they determine the location and size
* of the binary data in the memory (in kernel space).
*
* VA start and size are aligned to PAGE_SIZE.
*
* Returns a pointer to the new VMA structure.
* If not possible (e.g. no available slot, or address range
* is overlapping with the existing VMAs), returns NULL.
*/
struct vma *vma_insert(struct env *env, int type, void *mem_va, size_t mem_size,
    int perm, void *file_va, uint64_t file_size) {
    cprintf("[VMA_INSERT] start\n");

    struct vma *tmp, *new_vma;
    struct vma *vma = env->vma;

    uintptr_t va_start = ROUNDDOWN((uintptr_t) mem_va, PAGE_SIZE);
    uintptr_t va_end = ROUNDUP((uintptr_t) mem_va + mem_size, PAGE_SIZE);

    // Get new free vma from end of list
    new_vma = vma_get_last(env->vma);

    // No available slot - return NULL
    if (new_vma == NULL || new_vma->type != VMA_UNUSED) {
        return NULL;
    }

    // Update new vma
    new_vma->type = type;
    new_vma->va = (void *)va_start;
    new_vma->len = va_end - va_start;
    new_vma->perm = perm;
    new_vma->mem_va = mem_va;
    new_vma->file_va = file_va;
    new_vma->mem_size = mem_size;
    new_vma->file_size = file_size;

    // Disconnect VMA from the end of VMA list
    (new_vma->prev)->next = NULL;

    // Insert VMA in VMAs list
    // If all VMAs are unused or the new VMA has the smallest VA, append it in front
    if (vma->type == VMA_UNUSED || va_end < (uintptr_t) vma->va) {
        vma->prev = new_vma;
        new_vma->next = vma;
        new_vma->prev = NULL;
        env->vma = new_vma;
        return new_vma;
    }

    // Try to locate a place for VMA in the sorted VMAs list
    while (vma->next != NULL) {
        // Can we put it after the current VMA? Only if both is true:
        // 1. vma->end <= new_vma->start
        // 2. new_vma->end <= vma->next->start OR vma->next is unused
        if (((uintptr_t) vma->va + vma->len <= va_start) &&
            (((vma->next)->type == VMA_UNUSED) || (va_end < (uintptr_t) (vma->next)->va))) {

            tmp = vma->next;
            vma->next = new_vma;
            new_vma->prev = vma;
            tmp->prev = new_vma;            
            new_vma->next = tmp;

            return new_vma;
        }
        vma = vma->next;
    }

    // Append at the end if vma->end <= new_vma->start
    if ((uintptr_t) vma->va + vma->len <= va_start) {
        vma->next = new_vma;
        new_vma->prev = vma;
        new_vma->next = NULL; 
        return new_vma;
    }

    // Part of the address range is already in use, couldn' insert VMA
    cprintf("[VMA_INSERT] could not insert\n");
    return NULL;
}

// Find a free piece of virt mem of size size
// assumes size to be aligned
// returns -1 if there is no free slot or no contiguous region
uintptr_t vma_get_vmem(size_t size, struct vma *vma) {
    // Get virt mem before first vma or at 0 if empty
    if (vma->type == VMA_UNUSED) {
        return 0;
    } else if (size <= (uintptr_t) vma->va) {
        return (uintptr_t) vma->va - size;
    }

    while (vma->next != NULL) {
        // Append to end of list
        if ((vma->next)->type == VMA_UNUSED) {
            // Not enough space to KERNEL_VMA
            if (size > USER_TOP - ROUNDUP((uintptr_t) vma->va + vma->len, PAGE_SIZE)) {
                break;
            } else {
                return ROUNDUP((uintptr_t) vma->va + vma->len, PAGE_SIZE);
            }
        }
        // Found a hole after this vma
        else if (size <= (uintptr_t) (vma->next)->va - 
                 ROUNDUP((uintptr_t) vma->va + vma->len, PAGE_SIZE)) {
            return ROUNDUP((uintptr_t) vma->va + vma->len, PAGE_SIZE);
        }

        vma = vma->next;
    }

    return -1;
}

/**
* Allocates memory for region <va, va+size) and maps it in the pml4 of
* the given environment with the given permissions.
* Assumes size is already rounded to PAGE_SIZE.
* Panics if there is not enough physical memory to allocate.
*/
void vma_map_populate(uintptr_t va, size_t size, int perm, struct env *env) {
    uintptr_t virt_addr = va;
    struct page_info *page;

    // Alloc physical page for each virt mem page and map it
    while (virt_addr < va + size) {
        page = page_alloc(ALLOC_ZERO);
        if (page == NULL) {
            panic("Out of memory in VMA_MAP_POPULATE");
        }
        // LAB 7
        inc_allocated_in_env(env);

        if (page_insert(env->env_pml4, page, (void *) virt_addr, perm) != 0) {
            panic("Could not map whole VMA in page tables with flag MAP_POPULATE\n");
        }
        // LAB 7
        add_reverse_mapping(env, (void *) virt_addr, page, perm);
        virt_addr += PAGE_SIZE;
    }
}

/**
* Unmaps the given range of virtual addresses from the environment
* page tables, possibly destroys page tables / page directories /
* page directory pointers and frees physical pages if necessary.
* Assume aligned addresses.
*/
void vma_unmap(uintptr_t va, size_t size, struct env *env) {
    uintptr_t vi;
    struct page_info *page;
    struct page_table *pdp = NULL;
    struct page_table *pd = NULL;
    struct page_table *pt = NULL;
    physaddr_t *entry;
    int is_empty = 1;
    int i;

    // Remove and unmap the actual entries
    for (vi = va; vi < va + size; vi += PAGE_SIZE) {
        page_remove(env->env_pml4, (void *)vi);
        remove_reverse_mapping(env, (void *)vi, NULL);
    }

    // Start to remove page tables, then page dir tables, then page dir pointer tables
    for (i = 0; i < 3; i++) {
        // For each virtual address, get each page table and check if its empty
        // If its empty, delete that table and set the entry to 0 in the tlb
        for (vi = va; vi < va + size; vi += PAGE_SIZE) {
            // Pml4 entry
            entry = (env->env_pml4)->entries + PML4_INDEX((uintptr_t) va);
            if (!(*entry & PAGE_PRESENT)) {
                continue;
            }

            // PDP entry
            pdp = (struct page_table *)KADDR(PAGE_ADDR(*entry));

            if (i == 0 || i == 1) {
                entry = pdp->entries + PDPT_INDEX((uintptr_t) va);
                if (!(*entry & PAGE_PRESENT)) {
                    continue;
                }
                // Page dir entry
                pd = (struct page_table *)KADDR(PAGE_ADDR(*entry));
            }

            if (i == 0) {
                entry = pd->entries + PAGE_DIR_INDEX((uintptr_t) va);
                if (!(*entry & PAGE_PRESENT)) {
                    continue;
                }
                // Page table
                pt = (struct page_table *)KADDR(PAGE_ADDR(*entry));
            }
            
            // Fix pointers for rest of the code
            if (i == 1) {
                pt = pd;
            } else if (i == 2) {
                pt = pdp;
            }

            // Check the whole page table. If its empty, delete its entry in the page dir
            for (i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                if (pt->entries[i] & PAGE_PRESENT) {
                    is_empty = 0;
                    break;
                }
            }

            // Clear the entry and fix the higher level table entry
            if (is_empty) {
                page = pa2page(PAGE_ADDR(*entry));
                page_decref(page);
                *entry = 0;
                tlb_invalidate(env->env_pml4, entry);
            }
        }
    }
}

/**
* Returns a pointer to the last VMA from the VMAs list.
* It does not matter if its free or not.
*/
struct vma *vma_get_last(struct vma *vma) {
    if (vma == NULL) {
        return NULL;
    }

    while (vma->next != NULL) {
        vma = vma->next;
    }

    return vma;
}
