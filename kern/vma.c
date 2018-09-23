#include <kern/vma.h>

// MATTHIJS
// Given the environment and a virtual address,
// Return the vma that contains the virtual address
struct vma *vma_lookup(struct env *e, void *va) {
    struct vma *vma = e->vma;
    uintptr_t virt_addr = (uintptr_t) va;

    // Search linked list of vma's for correct vma
    while (vma != NULL) {
        // Check if this vma is the correct one
        if (virt_addr >= (uintptr_t) vma->va &&
            virt_addr < (uintptr_t)vma->va + vma->len) {
            break;
        }

        vma = vma->next;
    }

    return vma;
}

// MATTHIJS
// Try to insert a new vma into the vma list
int vma_insert(struct vma *new_vma, struct env *env) {
    struct vma *tmp;
    struct vma *vma = env->vma;
    uintptr_t va_end = (uintptr_t) new_vma->va + new_vma->len;

    // At vma limit
    if (env->vma_num == 128) {
        return 0;
    }

    // Insert vma in vma list
    // Vma list is empty
    if (vma == NULL) {
        env->vma = new_vma;
        new_vma->next = NULL;
        new_vma->prev = NULL;
    }
    // Append in front of list
    else if (va_end < (uintptr_t) vma->va) {
        vma->prev = new_vma;
        new_vma->next = vma;
        new_vma->prev = NULL;
        env->vma = new_vma;
    }
    // All other cases
    else {
        while (true) {
            // Append at end
            if (vma->next == NULL) {
                vma->next = new_vma;
                new_vma->next = NULL;
                new_vma->prev = vma;
                break;
            }
            // Append after vma and before next
            else if ((uintptr_t) new_vma->va >= (uintptr_t) vma->va + vma->len &&
                     va_end < (uintptr_t) (vma->next)->va) {
                tmp = vma->next;
                vma->next = new_vma;
                new_vma->prev = vma;
                tmp->prev = new_vma;            
                new_vma->next = tmp;
                break;
            }

            vma = vma->next;
        }
    }

    return 1;
}

// MATTHIJS: Something can go wrong if forgot some mem you shouldnt use
// Find a free piece of virt mem of size size
int vma_get_vmem(size_t size, struct vma *vma) {
    // No Vma alloced yet, so start at 0
    if (vma == NULL) {
        return 0;
    }

    // Get virt mem before first vma
    if (size <= (uintptr_t) vma->va) {
        return (uintptr_t) vma->va - size;
    }

    while (vma->next != NULL) {
        // Found a hole after this vma
        if (size <= (uintptr_t) (vma->next)->va - ((uintptr_t) vma->va + vma->len)) {
            return (uintptr_t) vma->va + vma->len;
        }

        vma = vma->next;
    }

    // Found a hole between last vma and start of KERNEL_VMA
    // MATTHIJS: user stack limit?
    if (size <= KERNEL_VMA - ((uintptr_t) vma->va + vma->len)) {
        return (uintptr_t) vma->va + vma->len;
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