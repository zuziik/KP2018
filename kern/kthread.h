#include <kern/env.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/lock.h>
#include <inc/x86-64/asm.h>

extern struct kthread *kthreads; 		/* All kernel threads 	*/
#define curkt (thiscpu->cpu_kthread)   	/* Current environment 	*/

// Managing kernel threads:

/* stubs.S */
void kthread_interrupt();			/* Yield from a kernel thread */
void kthread_restore_context(uint64_t rsp); /* Resume a kernel thread */
void kthread_end();					/* Finish a kernel thread, restore
									default values */

/* kthread.c */
void kthread_create(); 					/* Create a new kernel thread */
void kthread_save_rsp(uint64_t rsp);	/* Save RSP of the kernel thread
										(after an interrupt) */
uint64_t get_cpu_kernel_stack_top();	/* Get top of the stack of the
										main kernel thread (used to switch
										stacks after a kernel thread is
										interrupted or finished) */
void kthread_run(struct kthread *kt);	/* Start running a kthread */

// Kernel thread routines:
void kthread_dummy();