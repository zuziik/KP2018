/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>

envid_t fork(void)
{
    /* LAB 5: your code here. */
    int id = sys_fork();

    // child
    if (id == 0) {
    	const volatile struct env *e;
    	envid_t env_id = sys_getenvid();
   
	    for (e = envs; e < envs + NENV; ++e) {
	        if (e->env_id == env_id) {
	            thisenv = e;
	            break;
	        }
	    }

    	cprintf("[CCC] child: %d\n", thisenv->env_id);
    }
    // parent
    else {
    	cprintf("[CCC] parent: %d\n", thisenv->env_id);
    }

	return id;
}