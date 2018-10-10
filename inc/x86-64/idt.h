#pragma once

/* Interrupt numbers used for processor exceptions. */
#define INT_DIVIDE         0
#define INT_DEBUG          1
#define INT_NMI            2
#define INT_BREAK          3
#define INT_OVERFLOW       4
#define INT_BOUND          5
#define INT_INVALID_OP     6
#define INT_DEVICE         7
#define INT_DOUBLE_FAULT   8
#define INT_TSS            10
#define INT_NO_SEG_PRESENT 11
#define INT_SS             12
#define INT_GPF            13
#define INT_PAGE_FAULT     14
#define INT_FPU            16
#define INT_ALIGNMENT      17
#define INT_MCE            18
#define INT_SIMD           19
#define INT_SECURITY       30

/* Hardware IRQ numbers. */
#define IRQ_OFFSET         32
#define IRQ_TIMER          32
#define IRQ_KBD            33
#define IRQ_SERIAL         36
#define IRQ_SPURIOUS       39
#define IRQ_IDE            46
#define IRQ_ERROR          51

/* Software interrupt. */
#define INT_SYSCALL        128

#define IDT_GATE(x) ((x & 0xF) << 8)
#define IDT_STORAGE_SEG (1 << 12)
#define IDT_PRIVL(x) ((x & 0x3) << 13)
#define IDT_PRESENT (1 << 15)

#define IDT_TASK_GATE32 IDT_GATE(0x5)
#define IDT_INT_GATE16 IDT_GATE(0x6)
#define IDT_TRAP_GATE16 IDT_GATE(0x7)
#define IDT_INT_GATE32 IDT_GATE(0xE)
#define IDT_TRAP_GATE32 IDT_GATE(0xF)

#ifndef __ASSEMBLER__
#include <inc/x86-64/types.h>

struct idt_entry {
	uint16_t offset_lo;
	uint16_t sel;
	uint16_t flags;
	uint64_t offset_hi;
	uint16_t always0;
} __attribute__((packed));

struct idtr {
	uint16_t limit;
	struct idt_entry *entries;
} __attribute__((packed));

// Interrupt frame used for envs
struct int_frame {
    uint64_t ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

// Variant of int_frame for kernel threads with less variables
struct kthread_frame {
    uint64_t ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rflags;
    uint64_t rip;
};

static inline void set_idt_entry(struct idt_entry *entry, void *offset,
    unsigned flags, uint16_t sel)
{
    entry->offset_lo = (uintptr_t)offset & 0xFFFF;
    entry->offset_hi = ((uintptr_t)offset >> 16) & 0xFFFFFFFFFFFF;
    entry->flags = flags;
    entry->sel = sel;
}

static inline void load_idt(struct idtr *idtr)
{
	asm volatile("lidt (%0)" :: "r" (idtr));
}
#endif /* !defined(__ASSEMBLER__) */

