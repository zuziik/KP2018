#include <kern/env.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/lock.h>
#include <inc/x86-64/asm.h>

extern struct kthread *kthreads; 		/* All kernel threads 	*/
#define curkt (thiscpu->cpu_kthread)   	/* Current environment 	*/

// Managing kernel threads:
void kthread_interrupt();			/* stubs.S, save context when interrupted */
void kthread_create(); 					/* Create a new kernel thread */
void kthread_restore_context(uint64_t rsp); /* stubs.S, start to run kthread */
void kthread_end();
uint64_t get_cpu_kernel_stack_top();
void kthread_save_rsp(uint64_t rsp);
void kthread_run(struct kthread *kt);	/* Start running a kthread */
void kthread_dummy();					/* Dummy kthread function */

// Functions which a kernel thread can execute:
void kthread_dummy();