#include <inc/lib.h>

int g[1024];

void umain(int argc, char **argv)
{
    envid_t parent_id = thisenv->env_id;
    envid_t child_id;
    uint32_t orig_pte, new_pte;

    g[0] = thisenv->env_id;
    orig_pte = user_pt[(uintptr_t)g / PAGE_SIZE];
    cprintf("[%08x] g[0] = %x (pte: %x)\n", parent_id, g[0], orig_pte);

    child_id = fork();
    if (child_id < 0)
        panic("fork");

    if (child_id == 0) {
        /* Child */
        child_id = thisenv->env_id;
        assert(child_id != parent_id);
        cprintf("[%08x] g[0] = %x\n", child_id, g[0]);
        assert(g[0] == parent_id);

        new_pte = user_pt[(uintptr_t)g / PAGE_SIZE];
        cprintf("[%08x] g child pte: %x\n", child_id, new_pte);
        assert(PAGE_ADDR(new_pte) == PAGE_ADDR(orig_pte));
        assert((new_pte & PAGE_PRESENT) == PAGE_PRESENT);
        assert((new_pte & PAGE_WRITE) == 0);

        g[0] = child_id;

        new_pte = user_pt[(uintptr_t)g / PAGE_SIZE];
        cprintf("[%08x] g[0] = %x (pte: %x)\n", child_id, g[0], new_pte);
        assert(PAGE_ADDR(new_pte) != PAGE_ADDR(orig_pte));
    } else {
        /* Parent */
        sys_wait(child_id);
        cprintf("[%08x] g[0] = %x\n", parent_id, g[0]);
        assert(g[0] == parent_id);

        new_pte = user_pt[(uintptr_t)g / PAGE_SIZE];
        cprintf("[%08x] g parent pte: %x\n", parent_id, new_pte);
        assert(PAGE_ADDR(new_pte) == PAGE_ADDR(orig_pte));

        g[0] = 0;
        new_pte = user_pt[(uintptr_t)g / PAGE_SIZE];
        cprintf("[%08x] g[0] = %x (pte: %x)\n", parent_id, g[0], new_pte);
        assert(PAGE_ADDR(new_pte) == PAGE_ADDR(orig_pte));
    }
    cprintf("cowforktest completed.\n");
}

