#pragma once

#define CPU_ID       0
#define CPU_STATUS   4
#define CPU_ENV      8
#define CPU_TSS_RSP0 20
#define CPU_TSS_RSP1 28
#define CPU_TSS_RSP2 36
#define CPU_TSS_RSP3 44

#ifndef __ASSEMBLER__
#include <inc/types.h>
#include <inc/env.h>

#include <inc/x86-64/gdt.h>
#include <inc/x86-64/memory.h>

/* Maximum number of CPUs */
#define NCPU  8

/* Values of status in struct cpuinfo */
enum {
    CPU_UNUSED = 0,
    CPU_STARTED,
    CPU_HALTED,
};

/* Per-CPU state */
struct cpuinfo {
    uint8_t cpu_id;                /* Local APIC ID; index into cpus[] below */
    volatile unsigned cpu_status;  /* The status of the CPU */
    struct env *cpu_env;           /* The currently-running environment. */
    struct kthread *cpu_kthread;   /* Recently running kernel thread. */
    struct tss cpu_tss;            /* Used by x86 to find stack for interrupt */
};

/* Initialized in cpu.c */
extern struct cpuinfo cpus[NCPU];
extern int ncpu;                   /* Total number of CPUs in the system */
extern struct cpuinfo *bootcpu;    /* The boot-strap processor (BSP) */
extern physaddr_t lapicaddr;       /* Physical MMIO address of the local APIC */

/* Per-CPU kernel stacks */
extern unsigned char percpu_kstacks[NCPU][KSTACK_SIZE];

int cpunum(void);
#define thiscpu (&cpus[cpunum()])

void mp_init(void);
void lapic_init(void);
void lapic_startap(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
void lapic_ipi(int vector);

#endif /* !defined(__ASSEMBLER__) */
