#include <inc/assert.h>
#include <inc/stdio.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>
#include <inc/x86-64/idt.h>

#include <kern/env.h>
#include <kern/idt.h>
#include <kern/monitor.h>
#include <kern/syscall.h>

#include <kern/pmap.h>
#include <kern/vma.h>

#include <inc/string.h>

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr128(void);

static const char *int_names[256] = {
    [INT_DIVIDE] = "Divide-by-Zero Error Exception (#DE)",
    [INT_DEBUG] = "Debug (#DB)",
    [INT_NMI] = "Non-Maskable Interrupt",
    [INT_BREAK] = "Breakpoint (#BP)",
    [INT_OVERFLOW] = "Overflow (#OF)",
    [INT_BOUND] = "Bound Range (#BR)",
    [INT_INVALID_OP] = "Invalid Opcode (#UD)",
    [INT_DEVICE] = "Device Not Available (#NM)",
    [INT_DOUBLE_FAULT] = "Double Fault (#DF)",
    [INT_TSS] = "Invalid TSS (#TS)",
    [INT_NO_SEG_PRESENT] = "Segment Not Present (#NP)",
    [INT_SS] = "Stack (#SS)",
    [INT_GPF] = "General Protection (#GP)",
    [INT_PAGE_FAULT] = "Page Fault (#PF)",
    [INT_FPU] = "x86 FPU Floating-Point (#MF)",
    [INT_ALIGNMENT] = "Alignment Check (#AC)",
    [INT_MCE] = "Machine Check (#MC)",
    [INT_SIMD] = "SIMD Floating-Point (#XF)",
    [INT_SYSCALL] = "System Call",
};

static struct idt_entry entries[256];
static struct idtr idtr = {
    .limit = sizeof(entries) - 1,
    .entries = entries,
};

static const char *get_int_name(unsigned int_no)
{
    if (!int_names[int_no])
        return "Unknown Interrupt";

    return int_names[int_no];
}

void print_int_frame(struct int_frame *frame)
{
    cprintf("INT frame at %p\n", frame);

    /* Print the interrupt number and the name. */
    cprintf(" INT %u: %s\n",
        frame->int_no,
        get_int_name(frame->int_no));

    /* Print the error code. */
    switch (frame->int_no) {
    case INT_PAGE_FAULT:
        cprintf(" CR2 %p\n", read_cr2());
        cprintf(" ERR 0x%016llx (%s, %s, %s)\n",
                frame->err_code,
                frame->err_code & 4 ? "user" : "kernel",
                frame->err_code & 2 ? "write" : "read",
                frame->err_code & 1 ? "protection" : "not present");
        break;
    default:
        cprintf(" ERR 0x%016llx\n", frame->err_code);
    }

    /* Print the general-purpose registers. */
    cprintf(" RAX 0x%016llx"
            " RCX 0x%016llx"
            " RDX 0x%016llx"
            " RBX 0x%016llx\n"
            " RSP 0x%016llx"
            " RBP 0x%016llx"
            " RSI 0x%016llx"
            " RDI 0x%016llx\n"
            " R8  0x%016llx"
            " R9  0x%016llx"
            " R10 0x%016llx"
            " R11 0x%016llx\n"
            " R12 0x%016llx"
            " R13 0x%016llx"
            " R14 0x%016llx"
            " R15 0x%016llx\n",
            frame->rax, frame->rcx, frame->rdx, frame->rbx,
            frame->rsp, frame->rbp, frame->rsi, frame->rdi,
            frame->r8,  frame->r9,  frame->r10, frame->r11,
            frame->r12, frame->r13, frame->r14, frame->r15);

    /* Print the IP, segment selectors and the RFLAGS register. */
    cprintf(" RIP 0x%016llx"
            " RFL 0x%016llx\n"
            " CS  0x%04x"
            "            "
            " DS  0x%04x"
            "            "
            " SS  0x%04x\n",
            frame->rip, frame->rflags,
            frame->cs, frame->ds, frame->ss);
}

void idt_init(void)
{
    /* Set up the interrupt handlers in the IDT. */

    /* LAB 3: your code here. */

    // TODO what about the flags and selectors?
    set_idt_entry(&entries[INT_DIVIDE], isr0, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_DEBUG], isr1, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_NMI], isr2, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_BREAK], isr3,  IDT_PRESENT | IDT_PRIVL(3)| IDT_TRAP_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_OVERFLOW], isr4,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_BOUND], isr5,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_INVALID_OP], isr6,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_DEVICE], isr7,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_DOUBLE_FAULT], isr8,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_TSS], isr10,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_NO_SEG_PRESENT], isr11,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[INT_SS], isr12,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);         
    set_idt_entry(&entries[INT_GPF], isr13,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);     
    set_idt_entry(&entries[INT_PAGE_FAULT], isr14,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);     
    set_idt_entry(&entries[INT_FPU], isr16,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);     
    set_idt_entry(&entries[INT_ALIGNMENT], isr17,  IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);     
    set_idt_entry(&entries[INT_MCE], isr18, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);     
    set_idt_entry(&entries[INT_SIMD], isr19, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);

    set_idt_entry(&entries[INT_SYSCALL], isr128, IDT_PRESENT | IDT_PRIVL(3) | IDT_INT_GATE32, GDT_KCODE);

    idt_init_percpu();
}

void idt_init_percpu(void)
{
    /* Load the IDT. */
    load_idt(&idtr);
}

void int_dispatch(struct int_frame *frame)
{
    /* Handle processor exceptions. */
    /* LAB 3: your code here. */

    cprintf("Interrupt: %s, %d\n", get_int_name(frame->int_no), frame->int_no);

    if (frame->int_no == INT_PAGE_FAULT) {
        page_fault_handler(frame);
    }
    else if (frame->int_no == INT_BREAK) {
        print_int_frame(frame);
        monitor(frame);
    }
    else if (frame->int_no == INT_SYSCALL) {
        frame->rax = syscall((unsigned long) frame->rdi, (unsigned long) frame->rsi,
            (unsigned long) frame->rdx, (unsigned long) frame->rcx,
            (unsigned long) frame->r8, (unsigned long) frame->r9);
        return;
    }

    /* Unexpected trap: The user process or the kernel has a bug. */
    print_int_frame(frame);

    if (frame->cs == GDT_KCODE) {
        panic("unhandled interrupt in kernel");
    } else {
        env_destroy(curenv);
        return;
    }
}

void int_handler(struct int_frame *frame)
{
    /* The environment may have set DF and some versions of GCC rely on DF being
     * clear. */
    cprintf("Interrupt: %s, %d\n", get_int_name(frame->int_no), frame->int_no);
    asm volatile("cld" ::: "cc");

    /* Check that interrupts are disabled.
     * If this assertion fails, DO NOT be tempted to fix it by inserting a "cli"
     * in the interrupt path.
     */
    assert(!(read_rflags() & FLAGS_IF));

    cprintf("Incoming INT frame at %p\n", frame);

    if ((frame->cs & 3) == 3) {
        /* Interrupt from user mode. */
        assert(curenv);

        /* Copy interrupt frame (which is currently on the stack) into
         * 'curenv->env_frame', so that running the environment will restart at
         * the point of interrupt. */
        curenv->env_frame = *frame;

        /* Avoid using the frame on the stack. */
        frame = &curenv->env_frame;
    }

    /* Dispatch based on the type of interrupt that occurred. */
    int_dispatch(frame);

    /* Return to the current environment, which should be running. */
    assert(curenv && curenv->env_status == ENV_RUNNING);
    env_run(curenv);
}

void page_fault_handler(struct int_frame *frame)
{
    cprintf("[PAGE FAULT HANDLER] start\n");
    int is_user = (frame->err_code & 4) == 4;           // User or kernel space
    int is_protection = (frame->err_code & 1) == 1;     // Protection or non-present page
    void *fault_va;
    struct vma *vma;
    struct page_info *page;
    int alloc_flag = ALLOC_ZERO;

    /* Read the CR2 register to find the faulting address. */
    fault_va = read_cr2();
    uintptr_t fault_va_aligned = ROUNDDOWN((uintptr_t) fault_va, PAGE_SIZE);

    /* Handle kernel-mode page faults. */
    /* LAB 3: your code here. */
    /* MATTHIJS
     * always error except when trying to access user space page,
     * then do vma search and insert etc (when does this happen?)
     */
    // Kernel mode error
    if (!is_user) {
        panic("Page fault in kernel mode\n");
    }

    /* We have already handled kernel-mode exceptions, so if we get here, the
     * page fault has happened in user mode.
     */
    else {
        // User mode tries to read page is cant access
        if (is_protection) {
            panic("Page fault in user mode, protection violation\n");
        }
        // Page is not loaded, search in vma and map it in page tables
        else {
            // Get vma associated with faulting virt addr
            vma = vma_lookup(curenv, (void *)fault_va_aligned);
            if (vma == NULL) {
                panic("Page fault in user mode, try to access non existing page\n");
            } else {
                // There is a vma associated with this virt addr, now alloc the physical page
                page = page_alloc(ALLOC_ZERO);
                if (page == NULL) {
                    panic("Page fault error - couldn't allocate new page\n");
                } else if (page_insert(curenv->env_pml4, page, (void *) fault_va_aligned, vma->perm) != 0) {
                    panic("Page fault error - couldn't map new page\n");
                } else {
                    // Copy the binary from kernel space to user space, for anonymous memory nothing more has to be done
                    if (vma->type == VMA_BINARY) {
                        load_pml4((void *)PADDR(curenv->env_pml4));
                        uintptr_t va_src_start = (uintptr_t) vma->binary_start + ((uintptr_t) fault_va_aligned - (uintptr_t) vma->va);
                        uintptr_t binary_end = (uintptr_t) vma->binary_start + vma->binary_size;
                        uintptr_t va_src_end = ((va_src_start + PAGE_SIZE) < (binary_end)) ? (va_src_start + PAGE_SIZE) : binary_end;
                        memcpy((void *) fault_va, (void *) va_src_start, va_src_end - va_src_start);
                        load_pml4((void *)PADDR(kern_pml4));
                    }
                    return;
                }
            }
        }
    }

    /* Destroy the environment that caused the fault. */
    cprintf("[%08x] user fault va %p ip %p\n",
        curenv->env_id, fault_va, frame->rip);
    print_int_frame(frame);
    env_destroy(curenv);
}

// void page_fault_handler(struct int_frame *frame)
// {
//     cprintf("[PAGE FAULT HANDLER] start\n");
//     int is_user = (frame->err_code & 4) == 4;           // User or kernel space
//     int is_protection = (frame->err_code & 1) == 1;     // Protection or non-present page
//     void *fault_va;
//     struct vma *vma;
//     struct page_info *page;
//     int alloc_flag = ALLOC_ZERO;

//     /* Read the CR2 register to find the faulting address. */
//     fault_va = read_cr2();

//     /* Handle kernel-mode page faults. */
//     /* LAB 3: your code here. */
//     /* MATTHIJS
//      * always error except when trying to access user space page,
//      * then do vma search and insert etc (when does this happen?)
//      */
//     // Kernel mode error
//     if (!is_user) {
//         panic("Page fault in kernel mode\n");
//     }

//     /* We have already handled kernel-mode exceptions, so if we get here, the
//      * page fault has happened in user mode.
//      */
//     else {
//         // User mode tries to read page is cant access
//         if (is_protection) {
//             panic("Page fault in user mode, protection violation\n");
//         }
//         // Page is not loaded, search in vma and map it in page tables
//         else {
//             // Get vma associated with faulting virt addr
//             vma = vma_lookup(curenv, fault_va);
//             if (vma == NULL) {
//                 panic("Page fault in user mode, try to access non existing page\n");
//             }

            // Want to map a huge page or not
            // if (vma->perm & PAGE_HUGE) {
            //     alloc_flag |= ALLOC_HUGE;
            // }

            // // Map page
            // page = page_alloc(alloc_flag);
            // if (page_insert(curenv->env_pml4, page, (void *) fault_va, vma->perm) != 0) {
            //     panic("Could not map whole VMA in page tables with flag MAP_POPULATE\n");
            // } else {
            //     // Inserted without problem, dont destroy env
            //     return;
            // }
//         }
//     }

//     /* Destroy the environment that caused the fault. */
//     cprintf("[%08x] user fault va %p ip %p\n",
//         curenv->env_id, fault_va, frame->rip);
//     print_int_frame(frame);
//     env_destroy(curenv);
// }
