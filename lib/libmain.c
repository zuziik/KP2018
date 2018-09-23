/*
 * Called from entry.S to get us going.
 * entry.S already took care of defining envs, pages, uvpd, and uvpt.
 */

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct env *thisenv;
const volatile struct env *envs = (struct env *)USER_ENVS;
const volatile struct page_info *pages = (struct page_info *)USER_PAGES;

const volatile physaddr_t *user_pml4 = (physaddr_t *)USER_PML4;
const volatile physaddr_t *user_pdpt = (physaddr_t *)USER_PDPT;
const volatile physaddr_t *user_pd = (physaddr_t *)USER_PD;
const volatile physaddr_t *user_pt = (physaddr_t *)USER_PT;

const char *binaryname = "<unknown>";

void libmain(int argc, char **argv)
{
    const volatile struct env *e;
    envid_t env_id = sys_getenvid();

    thisenv = NULL;

    for (e = envs; e < envs + NENV; ++e) {
        if (e->env_id == env_id) {
            thisenv = e;
            break;
        }
    }

    assert(thisenv);

    /* Save the name of the program so that panic() can use it. */
    if (argc > 0)
        binaryname = argv[0];

    /* Call user main routine. */
    umain(argc, argv);

    /* Exit gracefully. */
    exit();
}

