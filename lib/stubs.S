#include <inc/x86-64/idt.h>

.section .text

.global do_syscall
do_syscall:
#ifdef LAB3_SYSCALL
    movq %rcx, %r10
    syscall
#else
    int $INT_SYSCALL
#endif
    retq
