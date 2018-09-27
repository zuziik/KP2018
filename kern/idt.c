#include <inc/assert.h>
#include <inc/stdio.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>
#include <inc/x86-64/idt.h>

#include <kern/env.h>
#include <kern/idt.h>
#include <kern/monitor.h>
#include <kern/sched.h>
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

extern void isr32(void);
extern void isr33(void);
extern void isr36(void);
extern void isr39(void);
extern void isr46(void);
extern void isr51(void);

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
    [IRQ_OFFSET...IRQ_OFFSET+16] = "Hardware interrupt",
    [INT_SYSCALL] = "System call",
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

    set_idt_entry(&entries[IRQ_TIMER], isr32, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[IRQ_KBD], isr33, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[IRQ_SERIAL], isr36, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[IRQ_SPURIOUS], isr39, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[IRQ_IDE], isr46, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);
    set_idt_entry(&entries[IRQ_ERROR], isr51, IDT_PRESENT | IDT_PRIVL(0) | IDT_INT_GATE32, GDT_KCODE);

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
    switch (frame->int_no) {
    case IRQ_TIMER:
        /* Handle clock interrupts. Don't forget to acknowledge the interrupt
         * using lapic_eoi() before calling the scheduler!
         */
        lapic_eoi();
        sched_yield();
        break;
    case IRQ_SPURIOUS:
        cprintf("Spurious interrupt on IRQ #7.\n");
        print_int_frame(frame);
        return;
    default: break;
    }

    cprintf("Interrupt: %s, %d\n", get_int_name(frame->int_no), frame->int_no);

    if (frame->int_no == INT_PAGE_FAULT) {
        page_fault_handler(frame);
        return;
    }
    else if (frame->int_no == INT_BREAK) {
        print_int_frame(frame);
        monitor(frame);
        return;
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
        cprintf("destroy in int_dispatch\n");
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
    // MATTHIJS: see env.c
    cprintf("%d\n", read_rflags());
    cprintf("FLAGS_IF must be 0!\n");
    cprintf("FLAGS_IF = %d\n", read_rflags() & FLAGS_IF);

    // assertion crashes if false, so 0, so if read_rflags() & FLAGS_IF == 1
    // assertion crashes if interrupts are enabled!
    assert(!(read_rflags() & FLAGS_IF));


    cprintf("Incoming INT frame at %p\n", frame);

    if ((frame->cs & 3) == 3) {
        /* Interrupt from user mode. */
        assert(curenv);

        /* Garbage collect if current enviroment is a zombie. */
        if (curenv->env_status == ENV_DYING) {
            env_free(curenv);
            curenv = NULL;
            sched_yield();
        }

        /* Copy interrupt frame (which is currently on the stack) into
         * 'curenv->env_frame', so that running the environment will restart at
         * the point of interrupt. */
        curenv->env_frame = *frame;

        /* Avoid using the frame on the stack. */
        frame = &curenv->env_frame;
    }

    /* Dispatch based on the type of interrupt that occurred. */
    int_dispatch(frame);

    /* If we made it to this point, then no other environment was scheduled, so
     * we should return to the current environment if doing so makes sense. */
    if (curenv && curenv->env_status == ENV_RUNNING)
        env_run(curenv);
    else
        sched_yield();
}

void page_fault_handler(struct int_frame *frame)
{
    cprintf("[PAGE FAULT HANDLER] start\n");
    int is_user = (frame->err_code & 4) == 4;           // User or kernel space
    int is_protection = (frame->err_code & 1) == 1;     // Protection or non-present page
    void *fault_va;

    /* Read the CR2 register to find the faulting address. */
    fault_va = read_cr2();
    uintptr_t fault_va_aligned = ROUNDDOWN((uintptr_t) fault_va, PAGE_SIZE);
    /* Handle kernel-mode page faults. */
    /* LAB 3: your code here. */

    // Kernel mode error
    if (!is_user) {
        // Kernel tries to read user space, load user space page
        if (fault_va_aligned < KERNEL_VMA) {
            if (page_fault_load_page((void *) fault_va_aligned)) {
                return;
            }
        } else {
            panic("Page fault in kernel mode - Kernel tries to read not mapped kernel space page\n");
        }
    }
    /* We have already handled kernel-mode exceptions, so if we get here, the
     * page fault has happened in user mode.
     * Protection violations always lead to destruction of env
     */
    else if (!is_protection) {
        // Page is not loaded, search in vma and map it in page tables
        if (page_fault_load_page((void *) fault_va_aligned)) {
            return;
        }
    }

    /* Destroy the environment that caused the fault. */
    cprintf("[%08x] user fault va %p ip %p\n",
        curenv->env_id, fault_va, frame->rip);
    print_int_frame(frame);
    env_destroy(curenv);
}

// Page fault has occured, load the page and map it
int page_fault_load_page(void *fault_va_aligned) {
    struct vma *vma;
    struct page_info *page;

    // Get vma associated with faulting virt addr
    vma = vma_lookup(curenv, (void *)fault_va_aligned);
    if (vma == NULL) {
        return 0;
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
                // Alligned start and end in userspace (dest)
                uintptr_t va_aligned_start = (uintptr_t) fault_va_aligned;
                uintptr_t va_aligned_end = (uintptr_t) fault_va_aligned + PAGE_SIZE;

                // Not alligned start and end of binary in kernel space
                uintptr_t va_file_start = (uintptr_t) vma->file_va;
                uintptr_t va_file_end = (uintptr_t) vma->file_va + vma->file_size;

                // Not alligned start and end in userspace (dest)
                uintptr_t va_mem_start = (uintptr_t) vma->mem_va;
                uintptr_t va_mem_end = (uintptr_t) vma->mem_va + vma->mem_size;

                uintptr_t va_src_start;
                uintptr_t va_src_end;
                uintptr_t va_dst_start;
                uintptr_t va_dst_end;

                uint64_t offset = va_file_start - va_mem_start;

                // Not alligned dest in user mem can not be before start of alligned page
                va_dst_start = (va_mem_start < va_aligned_start) ? va_aligned_start : va_mem_start;

                // Not alligned dest in user mem can not be after aligned end page
                va_dst_end = (va_mem_end > va_aligned_end) ? va_aligned_end : va_mem_end;

                // Source start cannot be before file start
                va_src_start = (va_file_start < va_aligned_start + offset) ? va_aligned_start + offset : va_file_start;

                // Source end cannot be after file end
                va_src_end = (va_file_end > va_aligned_end + offset) ? va_aligned_end + offset : va_file_end;

                // Allocated zeros
                if (va_src_end <= va_src_start)
                    return 1;

                uint64_t src_size = va_src_end - va_src_start;
                uint64_t dst_size = va_dst_end - va_dst_start;
                uint64_t copy_size = (src_size < dst_size) ? src_size : dst_size;

                load_pml4((void *)PADDR(curenv->env_pml4));
                memcpy((void *) va_dst_start, (void *) va_src_start, copy_size);
                load_pml4((void *)PADDR(kern_pml4));
            }
            return 1;
        }
    }
    return 0;
}
