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

struct kthread *kthreads = NULL;

void kthread_init_context(struct kthread *kt) {
    struct kthread_frame frame;
    memset(&frame, 0, sizeof (struct kthread_frame));
    frame.rflags = read_rflags();
    frame.rbp = kt->start_rbp;
    frame.rip = kt->start_rip;
    frame.ds = GDT_KDATA;
    memcpy((void *) (kt->start_rbp - sizeof(struct kthread_frame)), (void *) &frame, sizeof(struct kthread_frame));
    cprintf("copied\n");
    kt->rsp = kt->start_rbp - sizeof(struct kthread_frame);
}


// ZUZANA TODO maybe add arguments for the function
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

    kthreads[i].kt_id = id;
    kthreads[i].kt_status = ENV_RUNNABLE;

    kthreads[i].timeslice = MAX_WAITTIME;
    kthreads[i].prev_time = 0;

    kthreads[i].start_rip = (uint64_t) (start_routine);
    kthreads[i].start_rbp = KTHREAD_STACK_TOP - (KTHREAD_STACK_SIZE + KTHREAD_STACK_GAP) * i;

    kthread_init_context(&kthreads[i]);    
}


// Start a kernel thread
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

    // make this current kernel thread for this CPU
    curkt = kt;

    unlock_env();

    // start running
    kthread_restore_context(kt->rsp);
}

void kthread_save_rsp(uint64_t rsp) {
    env_lock_env();
    curkt->rsp = rsp;
    unlock_env();
}

uint64_t get_cpu_kernel_stack_top() {
    return KSTACK_TOP - (KSTACK_SIZE + KSTACK_GAP) * cpunum();
}

// Kernel thread interrupted itself, save state and call scheduler again
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

// Kernel thread is finished, reset state and kthread struct
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
    unlock_env();   // MATTHIJS: dont have to unlock
    sched_yield();
}


// ------------------------------------------------------------------------------
// ZUZANA TODO move this to another file kthreads.c or something where all
// entry functions for kernel threads will be
void kthread_dummy() {
    cprintf("[KTHREAD_DUMMY] start\n");
    
    kthread_interrupt();

    cprintf("[KTHREAD_DUMMY] end\n");

    kthread_end();
}

// ------------------------------------------------------------------------------
