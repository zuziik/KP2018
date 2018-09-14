#include <kern/pmap.h>

// Every leaf from the pml4 virtual address tree must either be 
// writable or executable, not both. This function enforces this.
void enable_wx(void) {
    cprintf("[ENABLE_WX] START\n");
    struct page_table *pml4, *pdpt, *pgdir, *pt;
    size_t s, t, u, v;

    pml4 = kern_pml4;

    for (s = 0; s < PAGE_TABLE_ENTRIES; ++s) {
        if (s == PML4_INDEX(USER_PML4))
            continue;

        if (!(pml4->entries[s] & PAGE_PRESENT))
            continue;

        pdpt = (void *)(KERNEL_VMA + PAGE_ADDR(pml4->entries[s]));

        for (t = 0; t < PAGE_TABLE_ENTRIES; ++t) {
            if (!(pdpt->entries[t] & PAGE_PRESENT))
                continue;

            pgdir = (void *)(KERNEL_VMA + PAGE_ADDR(pdpt->entries[t]));

            for (u = 0; u < PAGE_TABLE_ENTRIES; ++u) {
                if (!(pgdir->entries[u] & PAGE_PRESENT))
                    continue;

                if ((pgdir->entries[u] & PAGE_HUGE)) {
                    if ((pgdir->entries[u] & (PAGE_NO_EXEC | PAGE_WRITE)) ==
                        PAGE_WRITE) {
                        pgdir->entries[u] |= PAGE_NO_EXEC;

                    }
                    continue;
                }

                pt = (void *)(KERNEL_VMA + PAGE_ADDR(pgdir->entries[u]));

                for (v = 0; v < PAGE_TABLE_ENTRIES; ++v) {
                    if (!(pt->entries[v] & PAGE_PRESENT))
                        continue;

                    if ((pt->entries[v] & (PAGE_NO_EXEC | PAGE_WRITE)) ==
                        PAGE_WRITE) {
                        pt->entries[v] |= PAGE_NO_EXEC;
                    }
                }
            }
        }
    }
}

// Enforce write or execute on elf segments based on header
void enable_wx_elf(struct elf *elf_hdr) {
    cprintf("[ENABLE_WX_ELF] START\n");
    physaddr_t *addr;
    struct elf_proghdr *prog_hdr, next;
    prog_hdr = (struct elf_proghdr *)((char *)elf_hdr + elf_hdr->e_phoff);
    int i = 0;

    // Loop through all segments
    for (i = 1; i < elf_hdr->e_phnum; i++) {
        next = prog_hdr[i];
        addr = page_walk(kern_pml4, (void *)next.p_va, 0);
        if (next.p_flags & PAGE_WRITE) {
            *addr |= PAGE_NO_EXEC;
        }
    }
}
