/* Helper functions for locks
 * A few helper functions which do locking and unlocking in a certain order
 * and check if the locks are not already held by this cpu
 * 
 * Each lock function here has a counter function which does the complete opposite
 * One lock function is called at the start of some routine and the other is called
 * at the end of the routine and restores the locks like how they where before its 
 * counter was called at the beginning of the routine. 
 * For this the lock variable is used, so we can remember what we locked in this routine.
 * This prevents over-locking and removing locks where you shouldnt, like with a 
 * pagealloc function call within a pagealloc function.
 *
 * Note: functions for locking in pmap.c use master_lock for protection during
 *       init'ing of all cpu's, see main.c. Master_lock is only used in pmap.c for 
 *       this reason and will only be set during init'ing of all cpu's
 */

#include <kern/lock.h>

// For pmap.c: Try to unlock env and lock pagealloc
// Called at the start of a function
int lock_page_unlock_env() {
    int lock = 0;

    if (!holding(&master_lock)) {
        if (holding(&env_lock)) {
            lock += 1;
            unlock_env();
        }
    }

    if (!holding(&pagealloc_lock)) {
        lock += 2;
        lock_pagealloc();
    }

    return lock;
}

// For pmap.c: Try to unlock pagealloc and lock env
// Called at the end of a function
void unlock_page_lock_env(int lock) {
    if (lock > 1) {
        unlock_pagealloc();
    }

    if (lock == 1 || lock == 3) {
        lock_env();
    }
}

// For pmap.c: Try to unlock env
// Called at the start of a function
int pmap_unlock_env() {
    int lock = 0;

    if (!holding(&master_lock)) {
        if (holding(&env_lock)) {
            lock ++;
            unlock_env();
        }
    }        

    return lock;
}

// For pmap.c: Try to lock env
// Called at the end of a function
void pmap_lock_env(int lock) {
    if (lock) {
        lock_env();
    }
}

// Not for pmap.c: Try to lock env
// Called at the start of a function
int env_lock_env() {
    int kern = 0;

    if (!holding(&env_lock)) {
        kern++;
        lock_env();
    }

    return kern;
}

// Not for pmap.c: Try to unlock env
// Called at the end of a function
void env_unlock_env(int kern) {
    if (kern) {
        unlock_env();
    }
}

// swap.c: lock pagealloc when editing fields in page_info
int swap_lock_pagealloc() {
    int kern = 0;

    if (!holding(&pagealloc_lock)) {
        kern++;
        lock_pagealloc();
    }

    return kern;
}

// swap.c: unlock pagealloc when done editing fields in page_info
void swap_unlock_pagealloc(int kern) {
    if (kern) {
        unlock_pagealloc();
    }
}
