#include <inc/assert.h>
#include <inc/config.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/paging.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/spinlock.h>
#include <kern/kthread.h>

void sched_halt(void);

// Return the index of the env with id = env_id in the envs list
// Error: -1 -> env_id does not exist. This is used in syscall.c
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
    for (i = 0; i < NENV; i++) {
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
    cprintf("[SCHED_YIELD] start\n");
    if (!holding(&env_lock)) {
        lock_env();
    }

    struct env *env = NULL;
    int curenv_i, i, j;
    int64_t time = read_tsc();
    int64_t diff;

    // First time running something on this cpu
    if (curenv == NULL) {
        curenv_i = 0;
    }
    // Curenv was just destroyed
    else if (curenv->env_status == ENV_FREE) {
        curenv_i = get_env_index(curenv->env_id);
        curenv = NULL;
    }
    // Curenv just ran, check if its timeslice is done, else keep running it
    else if (curenv->env_status == ENV_RUNNING && curenv->env_cpunum != cpunum()) {
        curenv_i = get_env_index(curenv->env_id);

        // Update timeslice of curenv
        if (time >= curenv->prev_time)   
            diff =  time - curenv->prev_time;
        else {
            // 0x100000000 == MAXTIMESLICE in hex
            diff = time - curenv->prev_time + 0x100000000;
        }

        if (curenv->timeslice > diff) {
            curenv->timeslice -= diff;
            curenv->prev_time = time;

            // If env is still running and timeslice is not 0, continue executing
            if (curenv->env_status == ENV_RUNNING && curenv->pause < 0) {
                cprintf("[SCHED_YIELD] curenv resume\n");
                env_run(curenv);
            }
        }
        else {
            curenv->timeslice = 0;
            curenv->prev_time = time;
        }
    } 
    // A kernel thread just ran, curenv is the last env that ran before the kthread
    else {
        curenv_i = get_env_index(curenv->env_id);
    }

    // Corner case: curenv is last env so go circular
    i = (curenv_i + 1) % NENV;

    // Update timeslices of runnable kthreads
    for (j = 0; j < MAX_KTHREADS; j++) {
        if (kthreads[j].kt_id != -1 && kthreads[j].kt_status == ENV_RUNNABLE) {
            if (time >= kthreads[j].prev_time) 
                diff =  time - kthreads[j].prev_time;
            else {
                // 0x100000000 == MAXTIMESLICE in hex
                diff = time - kthreads[j].prev_time + 0x100000000;
            }

            kthreads[j].timeslice -= diff;
            kthreads[j].prev_time = time;
        }
    }

    // Try to schedule a kthread
    for (j = 0; j < MAX_KTHREADS; j++) {
        if (kthreads[j].kt_id != -1 && kthreads[j].timeslice < 0) {
            cprintf("[SCHED_YIELD] kthread\n");
            // kthread_run(&kthreads[j]);
            break;
        }
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
        if (&envs[i] != NULL && (&envs[i])->env_status == ENV_RUNNABLE && (&envs[i])->pause < 0) {
            env = &envs[i];
            break;
        }

        // Reached the end, go circular to 0
        i = (i + 1) % NENV;
    }

    // Edge case: There is only 1 env to run at the start
    if (i == curenv_i && curenv == NULL && (envs[curenv_i]).env_status == ENV_RUNNABLE) {
        env = &envs[curenv_i];
    }
    // No env found to run, run curenv again.
    // Edge case: a kernel thread just ran, a different cpu could have stolen your
    //            curenv so check if its still assigned to your cpu
    else if (i == curenv_i && curenv != NULL && curenv->pause < 0 &&
        (curenv->env_status == ENV_RUNNING || curenv->env_status == ENV_RUNNABLE) &&
        curenv->env_cpunum == cpunum()) {
        cprintf("[SCHED_YIELD] curenv new\n");
        env = curenv;
    }

    // Run the env
    if (env != NULL) {
        cprintf("[SCHED_YIELD] new\n");
        env->timeslice = MAXTIMESLICE;
        env->prev_time = time;
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

    /* Mark that this CPU is in the HALT state, so that when
     * timer interupts come in, we know we should re-acquire the
     * big kernel lock */
    xchg(&thiscpu->cpu_status, CPU_HALTED);

    /* Release the big kernel lock as if we were "leaving" the kernel */
    unlock_env();

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
