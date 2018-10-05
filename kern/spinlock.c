/* Mutual exclusion spin locks. */

#include <inc/types.h>
#include <inc/assert.h>
#include <inc/string.h>

#include <inc/x86-64/asm.h>

#include <kern/cpu.h>
#include <kern/spinlock.h>

#ifdef USE_BIG_KERNEL_LOCK
/* The big kernel lock */
struct spinlock kernel_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "kernel_lock"
#endif
};
#else
struct spinlock pagealloc_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "pagealloc_lock"
#endif
};
struct spinlock env_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "env_lock"
#endif
};
struct spinlock console_lock = {
#ifdef DEBUG_SPINLOCK
    .name = "console_lock"
#endif
};
#endif /* USE_BIG_KERNEL_LOCK */

/*
 * Check whether this CPU is holding the lock.
 */
int holding(struct spinlock *lock)       // MATTHIJS: removed static
{
    return lock->locked && lock->cpu == thiscpu;
}

void __spin_initlock(struct spinlock *lk, char *name)
{
    lk->locked = 0;
#ifdef DEBUG_SPINLOCK
    lk->name = name;
    lk->cpu = 0;
#endif
}

/*
 * Acquire the lock.
 * Loops (spins) until the lock is acquired.
 * Holding a lock for a long time may cause
 * other CPUs to waste time spinning to acquire it.
 */
void spin_lock(struct spinlock *lk)
{
#ifdef DEBUG_SPINLOCK
    if (holding(lk))
        panic("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
#endif

    /* The xchg is atomic.
     * It also serializes, so that reads after acquire are not reordered before
     * it. */
    while (xchg(&lk->locked, 1) != 0)
        asm volatile ("pause");

    /* Record info about lock acquisition for debugging. */
#ifdef DEBUG_SPINLOCK
    lk->cpu = thiscpu;
#endif
}

/*
 * Release the lock.
 */
void spin_unlock(struct spinlock *lk)
{
#ifdef DEBUG_SPINLOCK
    if (!holding(lk)) {
        cprintf("CPU %d cannot release %s: held by CPU %d\nAcquired at:",
            cpunum(), lk->name, lk->cpu->cpu_id);
        panic("spin_unlock");
    }

    lk->cpu = 0;
#endif

    /* The xchg serializes, so that reads before release are
     * not reordered after it.  The 1996 PentiumPro manual (Volume 3,
     * 7.2) says reads can be carried out speculatively and in
     * any order, which implies we need to serialize here.
     * But the 2007 Intel 64 Architecture Memory Ordering White
     * Paper says that Intel 64 and IA-32 will not move a load
     * after a store. So lock->locked = 0 would work here.
     * The xchg being asm volatile ensures gcc emits it after
     * the above assignments (and after the critical section). */
    xchg(&lk->locked, 0);
}
