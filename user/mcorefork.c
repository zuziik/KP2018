/* Fork a set of processes and display the core they are running on */

#include <inc/env.h>
#include <inc/lib.h>

void print_cpunum()
{
    thisenv = &envs[ENVX(sys_getenvid())];
    cprintf("[%08x] Running on cpu: %d\n", thisenv->env_id, thisenv->env_cpunum);
}

void umain(int argc, char **argv)
{
    if (0 > fork()) {
        cprintf("First fork() failed.\n");
    }
    if (0 > fork()) {
        cprintf("Second fork() failed.\n");
    }
    if (0 > fork()) {
        cprintf("Third fork() failed.\n");
    }
    print_cpunum();
}
