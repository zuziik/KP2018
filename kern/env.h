#pragma once

#include <inc/env.h>
#include <kern/cpu.h>

extern struct env *envs;        /* All environments */
extern struct env *curenv;      /* Current environment */
#define curenv (thiscpu->cpu_env)   /* Current environment */

void env_init(void);
void env_init_percpu(void);
int  env_alloc(struct env **e, envid_t parent_id);
void env_free(struct env *e);
void env_create(uint8_t *binary, enum env_type type);
void env_destroy(struct env *e); /* Does not return if e == curenv */

int  envid2env(envid_t envid, struct env **env_store, bool checkperm);
/* The following two functions do not return */
void env_run(struct env *e) __attribute__((noreturn));
void env_pop_frame(struct int_frame *frame) __attribute__((noreturn));

struct vma *vma_lookup(struct env *e, void *va);

/* Without this extra macro, we couldn't pass macros like TEST to ENV_CREATE
 * because of the C pre-processor's argument prescan rule. */
#define ENV_PASTE3(x, y, z) x ## y ## z

#define ENV_CREATE(x, type)                                     \
    do {                                                        \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _start)[];   \
        env_create(ENV_PASTE3(_binary_obj_, x, _start),         \
               type);                                           \
    } while (0)
