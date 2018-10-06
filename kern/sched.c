#include <inc/assert.h>
#include <inc/config.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/paging.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/spinlock.h>

void sched_halt(void);

// Return the index of env in the envs list
int get_env_index(envid_t env_id) {
    int i;
    for (i = 0; i < NENV; i++) {
        if ((&envs[i])->env_id == env_id) {
            return i;
        }
    }

    return -1;
}

// Reset the pause variable for each env which was waiting on the env with env_id
void reset_pause(envid_t env_id) {
    int i;
    for (i = 0; i++; i < NENV) {
        if ((&envs[i])->pause == env_id) {
            (&envs[i])->pause = -1;
        }
    }
}

/*
 * Choose a user environment to run and run it.
 */
void sched_yield(void)
{
    if (holding(&kernel_lock)) {
        panic("HOLDING KERNEL LOCK IN SCHED!\n");
    }

    cprintf("[SCHED_YIELD] lock env start\n");
    lock_env();
    cprintf("[SCHED_YIELD] lock env finish\n");

    cprintf("[SCHED_YIELD] start\n\n");
    struct env *env = NULL;
    int curenv_i, i;
    int64_t time = read_tsc();
    int64_t diff;

    // Just destroyed the previous current env
    if (curenv == NULL) {
        curenv_i = get_env_index(env_free_list->env_id);

        // Curenv just finished so reset the envs which were paused
        reset_pause(env_free_list->env_id);
    } else {
        // curenv just ran succesfully
        curenv_i = get_env_index(curenv->env_id);

        // Update timeslice of curenv
        if (time >= curenv->prev_time)   
            diff =  time - curenv->prev_time;
        else
            diff = time - curenv->prev_time + 0x5000000000;

        if (curenv->timeslice > diff) {
            curenv->timeslice -= diff;
            curenv->prev_time = time;

            // If env is still running and timeslice is not 0, continue executing
            if (curenv->env_status == ENV_RUNNING && curenv->pause < 0) {
                env_run(curenv);
                return;
            }
        }
        else {
            curenv->timeslice = 0;
            curenv->prev_time = time;
        }
    }

    // Corner case: curenv is last env so go circular
    i = (curenv_i + 1) % NENV;

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
        if (&envs[i] != NULL && (&envs[i])->env_status == ENV_RUNNABLE && (&envs[i])->pause < 0) {
            env = &envs[i];
            break;
        }

        // Reached the end, go circular to 0
        i = (i + 1) % NENV;
    }

    // No runnable envs found, use current if running or runnable
    if (i == curenv_i && curenv != NULL && curenv->pause < 0 &&
        (curenv->env_status == ENV_RUNNING || curenv->env_status == ENV_RUNNABLE)) {
        cprintf("[AAA] use curenv\n");
        env = curenv;
    }

    // Run the env
    if (env != NULL) {
        cprintf("[AAA] [CCC] new - %d\n", env->env_id);
        env->timeslice = 5000000000;
        env->prev_time = time;

        env_run(env);
        return;
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

    /* Mark that this CPU is in the HALT state, so that when
     * timer interupts come in, we know we should re-acquire the
     * big kernel lock */
    xchg(&thiscpu->cpu_status, CPU_HALTED);

    /* Release the big kernel lock as if we were "leaving" the kernel */
    cprintf("[SCHED_HALT] unlock env start\n");
    unlock_env();
    cprintf("[SCHED_HALT] unlock env finish\n");

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
