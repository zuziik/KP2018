/*
 * Simple command-line kernel monitor useful for controlling the kernel and
 * exploring the system interactively.
 */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <inc/x86-64/asm.h>

#include <kern/console.h>
#include <kern/monitor.h>

#define CMDBUF_SIZE 80  /* enough for one VGA text line */

struct command {
    const char *name;
    const char *desc;
    /* return -1 to force monitor to exit */
    int (*func)(int argc, char** argv, struct int_frame* tf);
};

static struct command commands[] = {
    { "help", "Display this list of commands", mon_help },
    { "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "backtrace", "Display stack backtrace", mon_backtrace },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int mon_help(int argc, char **argv, struct int_frame *frame)
{
    int i;

    for (i = 0; i < NCOMMANDS; i++)
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    return 0;
}

int mon_kerninfo(int argc, char **argv, struct int_frame *frame)
{
    extern char _start[], entry[], etext[], edata[], end[];

    cprintf("Special kernel symbols:\n");
    cprintf("  _start                  %08x (phys)\n", _start);
    cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNEL_VMA);
    cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNEL_VMA);
    cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNEL_VMA);
    cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNEL_VMA);
    cprintf("Kernel executable memory footprint: %dKB\n",
        ROUNDUP(end - entry, 1024) / 1024);
    return 0;
}

int mon_backtrace(int argc, char **argv, struct int_frame *frame)
{
    int i;
    uintptr_t *rbp = read_rbp();
    cprintf("Stack backtrace:\n");
    while (rbp) {
        uintptr_t rip = *(rbp + 1);
        //struct rip_debuginfo info;
        uintptr_t *args = rbp + 2;
        size_t nargs;

        cprintf("  RIP: %08x ", rip);
        
        /* TODO: implement support for DWARF. */
        cprintf("<no debug info>\t");
        nargs = 6;
        cprintf("  RBP: %08x ", rbp);
        if (nargs)
            cprintf("  args ");
        for (i = 0; i < nargs; i++)
            cprintf("%08x ", args[i]);
        cprintf("\n");
        rbp = (uintptr_t *)*rbp;
    }
    return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int runcmd(char *buf, struct int_frame *frame)
{
    int argc;
    char *argv[MAXARGS];
    int i;

    /* Parse the command buffer into whitespace-separated arguments */
    argc = 0;
    argv[argc] = 0;
    while (1) {
        /* gobble whitespace */
        while (*buf && strchr(WHITESPACE, *buf))
            *buf++ = 0;
        if (*buf == 0)
            break;

        /* save and scan past next arg */
        if (argc == MAXARGS-1) {
            cprintf("Too many arguments (max %d)\n", MAXARGS);
            return 0;
        }
        argv[argc++] = buf;
        while (*buf && !strchr(WHITESPACE, *buf))
            buf++;
    }
    argv[argc] = 0;

    /* Lookup and invoke the command */
    if (argc == 0)
        return 0;
    for (i = 0; i < NCOMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0)
            return commands[i].func(argc, argv, frame);
    }
    cprintf("Unknown command '%s'\n", argv[0]);
    return 0;
}

void monitor(struct int_frame *frame)
{
    char *buf;

    cprintf("Welcome to the JOS kernel monitor!\n");
    cprintf("Type 'help' for a list of commands.\n");

    while (1) {
        buf = readline("K> ");
        if (buf != NULL)
            if (runcmd(buf, frame) < 0)
                break;
    }
}
