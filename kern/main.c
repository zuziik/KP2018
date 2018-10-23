#include <kern/console.h>
#include <kern/env.h>
#include <kern/idt.h>
#include <kern/gdt.h>
#include <kern/monitor.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/syscall.h>
#include <kern/ide.h>

#include <inc/boot.h>
#include <inc/stdio.h>
#include <inc/string.h>

#include <kern/spinlock.h>
#include <kern/sched.h>
#include <kern/kthread.h>
#include <kern/swap.h>

static void boot_aps(void);

void kmain(struct boot_info *boot_info)
{
    extern char edata[], end[];

    /* Before doing anything else, complete the ELF loading process.
     * Clear the uninitialized global data (BSS) section of our program.
     * This ensures that all static/global variables start out zero. */
    memset(edata, 0, end - edata);

    /* Initialize the console.
     * Can't call cprintf until after we do this! */
    cons_init();
    cprintf("\n");

    /* Lab 1 memory management initialization functions */
    mem_init(boot_info);

    /* Lab 3 user environment initialization functions */
    gdt_init();
    idt_init();
    syscall_init();
    env_init();

    /* Lab 5 initialization functions */
    mp_init();
    lapic_init();
    pic_init();

    /* Lab 7: initialization functions */
    ide_init();
    swap_init();

    /* Acquire the big kernel lock before waking up APs.
     * LAB 6: your code here. */
    /* Master: prevent functions in pmap.c to unlock the env lock
     * Env: We are creating envs so stop other cpu's from scheduling until all
     *      envs are created */
    lock_master();
    lock_env();

    /* Starting non-boot CPUs */
    boot_aps();

#if defined(TEST)
    /* Don't touch -- used by grading script! */
    ENV_CREATE(TEST, ENV_TYPE_USER);
#else
    /* Touch all you want. */
    // A few different programs to do testing with
    ENV_CREATE(user_divzero, ENV_TYPE_USER);
    ENV_CREATE(user_vmatest, ENV_TYPE_USER);
    ENV_CREATE(user_mapunmap, ENV_TYPE_USER);
    ENV_CREATE(user_softint, ENV_TYPE_USER);
    ENV_CREATE(user_mapunmap, ENV_TYPE_USER);
    ENV_CREATE(user_mapunmap, ENV_TYPE_USER);
    ENV_CREATE(user_vmatest, ENV_TYPE_USER);
    ENV_CREATE(user_mempress, ENV_TYPE_USER);
#endif

    // Create some kernel threads
    // kthread_create(&kthread_dummy);
    // kthread_create(&kthread_dummy);
    // kthread_create(&kthread_dummy);

    // LAB 7
    // kthread_create(&kthread_swap);

    /* The init work for all cpu's and workload is finished, give the locks back
     * The master lock will never be used again */
    unlock_env();
    unlock_master();

    /* Start running an env */
    sched_yield();
}

/*
 * While boot_aps is booting a given CPU, it communicates the per-core
 * stack pointer that should be loaded by mpentry.S to that CPU in
 * this variable.
 */
void *mpentry_kstack;

/*
 * Start the non-boot (AP) processors.
 */
static void boot_aps(void)
{
    extern unsigned char boot_ap16[], boot_ap_end[];
    void *code;
    struct cpuinfo *c;

    /* Write entry code to unused memory at MPENTRY_PADDR */
    code = KADDR(MPENTRY_PADDR);
    memmove(code, KADDR((physaddr_t)boot_ap16), boot_ap_end - boot_ap16);

    /* Boot each AP one at a time */
    for (c = cpus; c < cpus + ncpu; c++) {
        if (c == cpus + cpunum())  /* We've started already. */
            continue;

        /* Tell mpentry.S what stack to use */
        mpentry_kstack = percpu_kstacks[c - cpus] + KSTACK_SIZE;
        /* Start the CPU at boot_ap16 */
        lapic_startap(c->cpu_id, PADDR(code));
        /* Wait for the CPU to finish some basic setup in mp_main() */
        while(c->cpu_status != CPU_STARTED)
            ;

        cprintf("[BOOT_APS] cpu %d is started\n", c->cpu_id);
    }
}

/*
 * Setup code for APs.
 */
void mp_main(void)
{
    /* Enable the NX-bit. */
    write_msr(MSR_EFER, read_msr(MSR_EFER) | MSR_EFER_NXE);

    /* We are in high EIP now, safe to switch to kern_pgdir */
    load_pml4((struct page_table *)PADDR(kern_pml4));
    cprintf("SMP: CPU %d starting\n", cpunum());

    lapic_init();
    gdt_init_percpu();
    idt_init_percpu();
    syscall_init_percpu();
    xchg(&thiscpu->cpu_status, CPU_STARTED); /* tell boot_aps() we're up */

    /*
     * Now that we have finished some basic setup, call sched_yield()
     * to start running processes on this CPU.  But make sure that
     * only one CPU can enter the scheduler at a time!
     *
     * LAB 6: your code here.
     */
    sched_yield();

    /* Remove this after you finish the per-CPU initialization code. */
    // for (;;);
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void _panic(const char *file, int line, const char *fmt,...)
{
    va_list ap;

    if (panicstr)
        goto dead;
    panicstr = fmt;

    /* Be extra sure that the machine is in as reasonable state */
    __asm __volatile("cli; cld");

    va_start(ap, fmt);
    cprintf("kernel panic at %s:%d: ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);

dead:
    /* break into the kernel monitor */
    while (1)
        monitor(NULL);
}

/* Like panic, but don't. */
void _warn(const char *file, int line, const char *fmt,...)
{
    va_list ap;

    va_start(ap, fmt);
    cprintf("kernel warning at %s:%d: ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}
