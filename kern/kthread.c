/* Kernel thread functions
 * Includes 2 categories of functions:
 * - Managing the kernel threads and handling their execution
 * - The actual functions that the kernel threats are going to execute
 *
 * Big parts of the design of kernel threads are stolen from environments like
 * the status of a kthread (=kernel thread), id's, kernel thread list and
 * current kernelthread (=curkthread)
 *
 * Other parts are different like the timeslice (which indicates how long to kthread
 * has to wait before it will execute in stead of how long it can execute untill it has
 * to sleep which is the case with envs). Another example is the frame struct, env uses
 * int_frame but kernel threads use kthread_frame.
 *
 * Other info can be found in inc/env.h and inc/x86-64/idt.h
 */

#include <kern/kthread.h>
#include <kern/sched.h>
#include <kern/cpu.h>
#include <kern/idt.h>

struct kthread *kthreads = NULL;

/**
* Initialize the context of the kernel thread to the default values:
* RIP -> start of the kernel thread routine
* RSP, RBP -> top of the kernel thread stack
*/
void kthread_init_context(struct kthread *kt) {
    struct kthread_frame frame;
    memset(&frame, 0, sizeof (struct kthread_frame));
    frame.rflags = read_rflags();
    frame.rbp = kt->start_rbp;
    frame.rip = kt->start_rip;
    frame.ds = GDT_KDATA;
    memcpy((void *) (kt->start_rbp - sizeof(struct kthread_frame)), (void *) &frame, sizeof(struct kthread_frame));
    kt->rsp = kt->start_rbp - sizeof(struct kthread_frame);
}

/**
* Create a kernel thread
* For Lab7, support for the arguments might be added.
*/
void kthread_create(void *(*start_routine))  
{
    int id = 0;
    int i;

    // generate new id
    for (i = 0; i < MAX_KTHREADS; i++) {
        if (kthreads[i].kt_id == -1) {
            break;
        }

        id++;
    }

    if (i == MAX_KTHREADS) {
        panic("No free kthread!\n");
    }

    // Set new values
    kthreads[i].kt_id = id;

    // Set default values
    kthreads[i].kt_status = ENV_RUNNABLE;
    kthreads[i].timeslice = MAX_WAITTIME;
    kthreads[i].prev_time = 0;

    // Save default instruction pointer and base pointer
    kthreads[i].start_rip = (uint64_t) (start_routine);
    kthreads[i].start_rbp = KTHREAD_STACK_TOP - (KTHREAD_STACK_SIZE + KTHREAD_STACK_GAP) * i;

    // Initialize the context
    kthread_init_context(&kthreads[i]);    
}

/*
* Resume execution of the kernel thread.
* The thread context from the previous run (or the initial
* context) is stored on the stack
*/
void kthread_run(struct kthread *kt)
{
    cprintf("[KTHREAD_RUN] start\n");

    int lock = env_lock_env();
    kt->kt_status = ENV_RUNNING;

    // Set env to runnable
    if (curenv != NULL && curenv->env_status == ENV_RUNNING && curenv->env_cpunum == cpunum()) {
        cprintf("[kthread_run] set curenv runnable!");
        curenv->env_status = ENV_RUNNABLE;
    }

    // Make this current kernel thread for this CPU
    curkt = kt;

    unlock_env();

    // Restore the context and resume execution doesn't return)
    kthread_restore_context(kt->rsp);
}

/**
* Save current stack pointer for the kernel thread currently
* running on this CPU. The stack pointer is important because
* all the context information for the thread is stored on the stack.
*/
void kthread_save_rsp(uint64_t rsp) {
    env_lock_env();
    curkt->rsp = rsp;
    unlock_env();
}

/**
* Get top of the main kernel thread stack for this cpu,
* i.e. not-kernel thread stacks.
* This is used when a kernel thread is executed or finished
* and we want to give the control back to the kernel main thread.
*/
uint64_t get_cpu_kernel_stack_top() {
    return KSTACK_TOP - (KSTACK_SIZE + KSTACK_GAP) * cpunum();
}

/**
* Reset kernel thread timing information and call a scheduler.
* This function is called after a kernel thread interrupts itself
* voluntarily. At this point, the stack has been switched to main
* kernel thread stack for this CPU.
*/
void kthread_yield() 
{
    cprintf("[KTHREAD_INTER] start\n");

    env_lock_env();

    // Reset waiting for kt and make it runnable again
    curkt->timeslice = MAX_WAITTIME;
    curkt->prev_time = read_tsc();
    curkt->kt_status = ENV_RUNNABLE;

    curkt = NULL;
    unlock_env();   // MATTHIJS: dont have to unlock

    sched_yield();
}

/**
* Reset kernel thread structure to prepare it for the next run
* (from the beginning) and call a scheduler.
* At this point, the stack has been switched to main kernel
* thread stack for this CPU.
*/
void kthread_finish()
{
    cprintf("[KTHREAD_FINISH] start\n");

    env_lock_env();

    // Reset waiting for kt and make it runnable again
    curkt->timeslice = MAX_WAITTIME;
    curkt->prev_time = read_tsc();
    curkt->kt_status = ENV_RUNNABLE;

    kthread_init_context(curkt);

    curkt = NULL;

    cprintf("ENDED\n");
    unlock_env();
    sched_yield();
}

// ------------------------------------------------------------------------------

/**
* Dummy function for testing kernel threads. This might be moved into another
* file for Lab7.
*/
void kthread_dummy() {
    cprintf("[KTHREAD_DUMMY] start\n");
    
    struct cpuinfo *c;
    for (c = cpus; c < cpus + ncpu; c++) {
        cprintf("cpuid = %d\n", c->cpu_id);
        if (c->cpu_id != thiscpu->cpu_id) {
            // lapic_ipi(200);
            lapic_test(c->cpu_id, IRQ_KILL);
        }
    }

    kthread_interrupt();

    cprintf("[KTHREAD_DUMMY] end\n");

    kthread_end();
}

// ------------------------------------------------------------------------------
