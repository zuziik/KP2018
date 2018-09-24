#include <kern/vma.h>

// MATTHIJS
// Given the environment and a virtual address,
// Return the vma that contains the virtual address
struct vma *vma_lookup(struct env *env, void *va) {
    struct vma *vma = env->vma;
    uintptr_t virt_addr = (uintptr_t) va;

    // Search linked list of vma's for correct vma
    while (vma != NULL) {
        // Vma is free so not found
        if (vma->is_free) {
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

// MATTHIJS
// Try to insert a new vma into the vma list
struct vma *vma_insert(struct env *env, int type, void *va, size_t len, int perm) {
    struct vma *tmp, *new_vma;
    struct vma *vma = env->vma;
    uintptr_t va_end = (uintptr_t) va + len;

    // Get new free vma from end of list
    new_vma = vma_get_last(env->vma);
    if (new_vma == NULL) {
        return NULL;
    }

    // Update new vma
    new_vma->type = type;
    new_vma->va = va;
    new_vma->len = len;             // MATTHIJS: allign size?
    new_vma->perm = perm;
    new_vma->is_free = 0;
    (new_vma->prev)->next = NULL;

    // Insert vma in vma list
    // Append in front
    if (vma->is_free || va_end < (uintptr_t) vma->va) {
        vma->prev = new_vma;
        new_vma->next = vma;
        new_vma->prev = NULL;
        env->vma = new_vma;
    }
    // All other cases
    else {
        while (true) {
            if (vma->next != NULL) {
                // Append at end of not free part
                // or between this and next vma
                if ((vma->next)->is_free || 
                    ((uintptr_t) new_vma->va >= (uintptr_t) vma->va + vma->len &&
                     va_end < (uintptr_t) (vma->next)->va)) {
                    tmp = vma->next;
                    vma->next = new_vma;
                    new_vma->prev = vma;
                    tmp->prev = new_vma;            
                    new_vma->next = tmp;
                    break;
                }
            } else {
                // Append at the complete end
                vma->next = new_vma;
                new_vma->prev = vma;
                new_vma->next = NULL;
                break;
            }

            vma = vma->next;
        }
    }

    return new_vma;
}

// MATTHIJS: Something can go wrong if forgot some mem you shouldnt use
// Find a free piece of virt mem of size size
uintptr_t vma_get_vmem(size_t size, struct vma *vma) {
    // Get virt mem before first vma or at 0 if empty
    if (vma->is_free) {
        return 0;
    } else if (size <= (uintptr_t) vma->va) {
        return (uintptr_t) vma->va - size;
    }

    while (vma->next != NULL) {
        // Append to end of list
        if ((vma->next)->is_free) {
            // Not enough space to KERNEL_VMA
            if (size > KERNEL_VMA - ((uintptr_t) vma->va + vma->len)) {
                break;
            } else {
                return (uintptr_t) vma->va + vma->len;
            }
        }
        // Found a hole after this vma
        else if (size <= (uintptr_t) (vma->next)->va - ((uintptr_t) vma->va + vma->len)) {
            return (uintptr_t) vma->va + vma->len;
        }

        vma = vma->next;
    }

    return -1;
}

// MATTHIJS
// Map whole vma directly into page tables
void vma_map_populate(uintptr_t va, size_t size, int perm, struct env *env) {
    uintptr_t virt_addr = va;
    size_t page_size = PAGE_SIZE;
    int alloc_flag = ALLOC_ZERO;
    struct page_info *page;

    // Check if huge pages are used
    // MATTHIJS: what if size % huge_page_size != 0?
    if (perm & PAGE_HUGE) {
        page_size *= 512;
        alloc_flag |= ALLOC_HUGE;
    }

    // Alloc physical page for each virt mem page and map it
    while (virt_addr < va + size) {
        page = page_alloc(alloc_flag);
        if (page_insert(env->env_pml4, page, (void *) virt_addr, perm) != 0) {
            panic("Could not map whole VMA in page tables with flag MAP_POPULATE\n");
        }
        virt_addr += page_size;
    }
}

void vma_unmap(uintptr_t va, size_t size, struct env *env) {
    return;
}

// Get the last vma from the list and check if its free
struct vma *vma_get_last(struct vma *vma) {
    if (vma == NULL) {
        return NULL;
    }

    while (vma->next != NULL) {
        vma = vma->next;
    }

    // Not free
    if (vma->is_free == 0) {
        return NULL;
    } else {
        return vma;
    }
}
