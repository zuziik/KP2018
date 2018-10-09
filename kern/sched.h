/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

/* This function does not return. */
void sched_yield(void) __attribute__((noreturn));

// Return the index in the envs list of an env with env_id
int get_env_index(envid_t env_id);

// The environment with id = env_id has finished, other envs which
// were waiting for this env to finish can now continue to run
void reset_pause(envid_t env_id);

#endif  /* !JOS_KERN_SCHED_H */
