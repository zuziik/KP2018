.section .text

#include <inc/x86-64/gdt.h>
#include <inc/x86-64/idt.h>
#include <inc/x86-64/memory.h>
#include <kern/cpu.h>

.macro ISR_NOERRCODE int_no
.global isr\int_no
isr\int_no:
    pushq $0
    pushq $\int_no
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE int_no
.global isr\int_no
isr\int_no:
    pushq $\int_no
    jmp isr_common_stub
.endm

ISR_NOERRCODE INT_DIVIDE
ISR_NOERRCODE INT_DEBUG
ISR_NOERRCODE INT_NMI
ISR_NOERRCODE INT_BREAK
ISR_NOERRCODE INT_OVERFLOW
ISR_NOERRCODE INT_BOUND
ISR_NOERRCODE INT_INVALID_OP
ISR_NOERRCODE INT_DEVICE
ISR_ERRCODE INT_DOUBLE_FAULT
ISR_ERRCODE INT_TSS
ISR_ERRCODE INT_NO_SEG_PRESENT
ISR_ERRCODE INT_SS
ISR_ERRCODE INT_GPF
ISR_ERRCODE INT_PAGE_FAULT
ISR_NOERRCODE INT_FPU
ISR_ERRCODE INT_ALIGNMENT
ISR_NOERRCODE INT_MCE
ISR_NOERRCODE INT_SIMD
ISR_NOERRCODE INT_SYSCALL

# lab 5, also see idt.c for new idt entries
ISR_NOERRCODE IRQ_TIMER
ISR_ERRCODE IRQ_KBD
ISR_ERRCODE IRQ_SERIAL 
ISR_ERRCODE IRQ_SPURIOUS
ISR_ERRCODE IRQ_IDE
ISR_ERRCODE IRQ_ERROR

isr_common_stub:
    /* LAB 3: your code here. */
    # cli 
    # clear interrupt flag, lab 5 - not needed if we use INT_TRAP for everything 
    # (plus, if we wanted to use something else, it wouldn't work)

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /*push %ds*/
    xor %rax, %rax
    movw %ds, %ax
    pushq %rax

    # ds, es -> GDT_KDATA
    movw $GDT_KDATA, %ax
    movw %ax, %ds
    movw %ax, %es

    movq %rsp, %rdi
    jmp int_handler

.global syscall64
syscall64:
    /* LAB 3: your code here. */
    jmp syscall_handler

.global iret64
iret64:
    /* LAB 3: your code here. */
    
    movq %rdi, %rsp
    
    popq %rax
    movw %ax, %ds
    movw %ax, %es

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    
    # clear int_no and err_no from the stack
    addq $0x10, %rsp
    
    iretq

.global sysret64
sysret64:
    /* LAB 3: your code here. */
    sysretq

# Voluntary interrupt of a kernel thread:
# saves context, switches stack to a kernel
# main thread stack and calls a scheduler
.global kthread_interrupt
kthread_interrupt:

    # rip should be pushed automatically if we use call
    pushfq

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /*push %ds*/
    xor %rax, %rax
    movw %ds, %ax
    pushq %rax

    # set current rsp to curkt struct
    movq %rsp, %rdi
    call kthread_save_rsp

    # switch to kernel stack on this cpu
    call get_cpu_kernel_stack_top
    movq %rax, %rsp
    movq %rax, %rbp

    jmp kthread_yield

# Finishes execution of a kernel thread, restores the default values,
# switches to a kernel main thread stack and calls a scheduler
.global kthread_end
kthread_end:
    
    # switch to kernel stack on this cpu
    call get_cpu_kernel_stack_top
    movq %rax, %rsp
    movq %rax, %rbp

    jmp kthread_finish

# Restores the context of the kernel thread and resumes its execution
.global kthread_restore_context
kthread_restore_context:

    movq %rdi, %rsp

    #pop %ds
    popq %rax
    movw %ax, %ds
    movw %ax, %es

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    
    popfq

    # rip is restored with retq    
    retq
    