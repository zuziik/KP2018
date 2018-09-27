#include <inc/assert.h>
#include <inc/config.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/paging.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Return the index of env in the envs list
int get_env_index(struct env *env) {
    int i;
    for (i = 0; i < NENV; i++) {
        if ((&envs[i])->env_id == env->env_id) {
            return i;
        }
    }

    panic("Env not in envs list\n");
}

/*
 * Choose a user environment to run and run it.
 */
void sched_yield(void)
{
    cprintf("[SCHED_YIELD] start\n\n");
    struct env *env = NULL;
    int curenv_i, i;

    // Just destroyed the previous current env
    if (curenv == NULL) {
        curenv_i = get_env_index(env_free_list);        // MATTHIJS: is this correct, made the free list non-static to access it
    } else {
        // curenv just ran succesfully
        curenv_i = get_env_index(curenv);
    }

    i = curenv_i + 1;
    // Corner case: curenv is last env so go circular
    if (i == NENV) {
        i = 0;
    }

    /*
     * Implement simple round-robin scheduling.
     *
     * Search through 'envs' for an ENV_RUNNABLE environment in
     * circular fashion starting just after the env this CPU was
     * last running.  Switch to the first such environment found.
     *
     * If no envs are runnable, but the environment previously
     * running on this CPU is still ENV_RUNNING, it's okay to
     * choose that environment.
     *
     * Never choose an environment that's currently running (on
     * another CPU, if we had, ie., env_status == ENV_RUNNING). 
     * If there are
     * no runnable environments, simply drop through to the code
     * below to halt the cpu.
     *
     * LAB 5: Your code here.
     */

    // Search for the first runnable env after curenv
    while (i != curenv_i) {
        // Found a different runnable env
        if (&envs[i] != NULL && (&envs[i])->env_status == ENV_RUNNABLE) {
            env = &envs[i];
            break;
        }

        // Reached the end, go circular to 0
        if (i == NENV - 1) {
            i = 0;
        } else {
            i++;
        }
    }

    // No runnable envs found, use current if running or runnable
    if (i == curenv_i && curenv != NULL && 
        (curenv->env_status == ENV_RUNNING || curenv->env_status == ENV_RUNNABLE)) {
        env = curenv;
    }

    // Run the env
    if (env != NULL) {
        env_run(env);
    } 

    /* sched_halt() never returns */
    sched_halt();
}

/*
 * Halt this CPU when there is nothing to do. Wait until the timer interrupt
 * wakes it up. This function never returns.
 */
void sched_halt(void)
{
    int i;

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor. */
    for (i = 0; i < NENV; i++) {
        if ((envs[i].env_status == ENV_RUNNABLE ||
             envs[i].env_status == ENV_RUNNING ||
             envs[i].env_status == ENV_DYING))
            break;
    }

    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        cprintf("Destroyed the only environment - nothing more to do!\n");
        while (1)
            monitor(NULL);
    }

    /* Mark that no environment is running on this CPU */
    curenv = NULL;
    load_pml4((struct page_table *)PADDR(kern_pml4));

    /* Reset stack pointer, enable interrupts and then halt. */
    asm volatile(
        "movq $0, %%rbp\n"
        "movq %0, %%rsp\n"
        "pushq $0\n"
        "pushq $0\n"
        "sti\n"
        "hlt\n"
        :: "a" (thiscpu->cpu_tss.rsp[0]));
}

