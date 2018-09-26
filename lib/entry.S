#include <inc/x86-64/memory.h>

/*  Entrypoint - this is where the kernel (or our parent environment)
 * starts us running when we are initially loaded into a new environment. */
.text
.global _start
_start:
    /*  See if we were started with arguments on the stack. */

    movabsq $USTACK_TOP, %rax
    cmpq %rax, %rsp
    jne args_exist

    /* If not, push dummy argc/argv arguments.
     * This happens when we are loaded by the kernel,
     * because the kernel does not know about passing arguments. */
    xorl %edi, %edi
    xorl %esi, %esi

args_exist:
    call libmain
1:  jmp 1b

