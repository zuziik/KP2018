/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>

envid_t fork(void)
{
    /* LAB 5: your code here. */
    // panic("fork not implemented");

    // Should also update things in " local" env info
    // Like envs[] or thisenv (which is curenv in userspace)
    cprintf("[CCC]\n");
	return sys_fork();
}