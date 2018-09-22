#pragma once 

#include <inc/syscall.h>

void sysret64(struct int_frame *frame);

void syscall_init(void);
int64_t syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
        uint64_t a4, uint64_t a5);
void syscall_init_percpu(void);
