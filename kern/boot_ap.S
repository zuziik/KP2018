#include <inc/x86-64/asm.h>
#include <inc/x86-64/gdt.h>
#include <inc/x86-64/memory.h>
#include <inc/x86-64/paging.h>

/* See COPYRIGHT for copyright information. */

###################################################################
# entry point for APs
###################################################################

# Each non-boot CPU ("AP") is started up in response to a STARTUP
# IPI from the boot CPU.  Section B.4.2 of the Multi-Processor
# Specification says that the AP will start in real mode with CS:IP
# set to XY00:0000, where XY is an 8-bit value sent with the
# STARTUP. Thus this code must start at a 4096-byte boundary.
#
# Because this code sets DS to zero, it must run from an address in
# the low 2^16 bytes of physical memory.
#
# boot_aps() (in init.c) copies this code to MPENTRY_PADDR (which
# satisfies the above restrictions).  Then, for each AP, it stores the
# address of the pre-allocated per-core stack in mpentry_kstack, sends
# the STARTUP IPI, and waits for this code to acknowledge that it has
# started (which happens in mp_main in init.c).
#
# This code is similar to boot/boot.S except that
#    - it does not need to enable A20
#    - it uses MPBOOTPHYS to calculate absolute addresses of its
#      symbols, rather than relying on the linker to fill them

#define BOOT_AP_PHYS(x) ((x) - boot_ap16 + MPENTRY_PADDR)

.code16
.global boot_ap16
boot_ap16:
    cli

    /* Enable protected mode. */
    movl %cr0, %eax
    orl $CR0_PM, %eax
    movl %eax, %cr0

    /* Load the global descriptor table and set up the segment registers. */
    lgdt BOOT_AP_PHYS(gdtr32)

    movw $GDT_KDATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ljmp $GDT_KCODE, $(BOOT_AP_PHYS(boot_ap32))

.code32
boot_ap32:
    /* Load the root of the page table hierarchy. */
    movl $pml4, %eax
    movl %eax, %cr3

    /* Enable physical address extensions to be able to access all physical
     * memory. This is mandatory to set up x86-64.
     */
    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    /* Enter compatibility mode (i.e. 32-bit long mode). */
    movl $MSR_EFER, %ecx
    rdmsr
    orl $MSR_EFER_LME, %eax
    wrmsr

    /* Enable paging to actually use the mapping we have set up. This mapping
     * is temporary as we will set up full paging in lab 2.
     */
    movl %cr0, %eax
    orl $CR0_PAGING, %eax
    movl %eax, %cr0

    /* On x86 both segmentation and paging are supported as models of memory
     * protection. For compatibility reasons we still have to set up a global
     * descriptor table with segment descriptors for both our code and data.
     * With the introduction of x86-64, a long mode flag has been added to the
     * segment descriptors to support both 32-bit (compatibility mode) and
     * 64-bit applications (long mode). Load a basic global descriptor table
     * and update the segment registers accordingly.
     */
    lgdt gdtr64

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    ljmp $0x08, $boot_ap64

.code64
boot_ap64:
    movabsq $(mpentry_kstack), %rax
    movq (%rax), %rsp
    xorl %ebp, %ebp

    movabsq $mp_main, %rax
    callq *%rax

    /* We are not supposed to get here, but if we do halt the system. */
    hlt

.section .data

.balign 8
gdt32:
    /* Null descriptor. */
    .word 0
    .word 0
    .byte 0
    .word 0
    .byte 0

    /* Kernel code descriptor. */
    .word 0xFFFF
    .word 0
    .byte 0
    .word GDT_KCODE_FLAGS | GDT_SIZE | GDT_GRANULARITY | GDT_LIMIT(0xF)
    .byte 0

    /* Kernel data descriptor. */
    .word 0xFFFF
    .word 0
    .byte 0
    .word GDT_KDATA_FLAGS | GDT_SIZE | GDT_GRANULARITY | GDT_LIMIT(0xF)
    .byte 0

gdtr32:
    .word . - gdt32 - 1
    .long gdt32

.global boot_ap_end
boot_ap_end:

