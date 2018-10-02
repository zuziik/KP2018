#include <kern/console.h>
#include <kern/env.h>
#include <kern/idt.h>
#include <kern/gdt.h>
#include <kern/monitor.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/syscall.h>

#include <inc/boot.h>
#include <inc/stdio.h>
#include <inc/string.h>

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
    lapic_init();
    pic_init();

#if defined(TEST)
    /* Don't touch -- used by grading script! */
    ENV_CREATE(TEST, ENV_TYPE_USER);
#else
    /* Touch all you want. */
    // ENV_CREATE(user_divzero, ENV_TYPE_USER);
    ENV_CREATE(user_yield, ENV_TYPE_USER);
    ENV_CREATE(user_yield, ENV_TYPE_USER);
    ENV_CREATE(user_yield, ENV_TYPE_USER);

#endif

    /* We only have one user environment for now, so just run it. */
    env_run(&envs[0]);
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

