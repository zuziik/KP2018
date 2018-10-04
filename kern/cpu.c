#include <inc/types.h>
#include <inc/string.h>
#include <inc/env.h>

#include <inc/x86-64/asm.h>
#include <inc/x86-64/memory.h>
#include <inc/x86-64/paging.h>

#include <kern/cpu.h>
#include <kern/pmap.h>

#include <inc/acpi.h>

struct cpuinfo cpus[NCPU];
struct cpuinfo *bootcpu;
int ismp;
int ncpu;

/* Per-CPU kernel stacks */
unsigned char percpu_kstacks[NCPU][KSTACK_SIZE]
__attribute__ ((aligned(PAGE_SIZE)));

void mp_init(void)
{
    struct acpi_rsdp *rsdp;
    struct acpi_madt *madt;
    struct acpi_madt_entry *entry;
    struct acpi_lapic *lapic;
    struct acpi_hdr *hdr;
    size_t i, len;
    char *p;

    bootcpu = &cpus[0];

    /*
     * Look for the Root System Description Pointer in known memory locations.
     * This is the root table for ACPI.
     */
    rsdp = acpi_find_rsdp();

    if (!rsdp)
        return;

    cprintf("Found RSDP at %p\n", rsdp);

    /* Find the Multiple APIC Description Table. */
    madt = acpi_find_table(rsdp, "APIC");

    if (!madt) {
        cprintf("No MADT found.\n");
        return;
    }

    cprintf("Found MADT at %p\n", madt);

    /*
     * Assume that we have SMP unless we can't properly parse the MADT.
     * Store the the register base for the local APIC, so we can configure
     * the local APIC for each CPU core.
     */
    ismp = 1;
    lapicaddr = madt->lapic_base;

    hdr = &madt->hdr;
    len = hdr->len - sizeof *madt;
    p = (char *)(madt + 1);

    /*
     * Parse through the entries of the MADT to find the entry for each local
     * APIC. Each local APIC indicates an individual CPU core. We can use this
     * to collect the CPU IDs and count the number of CPU cores.
     */
    for (i = 0; i < len; i += entry->len, p += entry->len) {
        entry = (struct acpi_madt_entry *)p;

        switch (entry->type) {
        case ACPI_MADT_LAPIC: {
                lapic = &entry->lapic;

                if (ncpu < NCPU) {
                    cpus[ncpu].cpu_id = lapic->cpu_id;
                    ncpu++;
                } else {
                    cprintf("SMP: too many CPUs, CPU %d disabled\n",
                        lapic->apic_id);
                }
            } break;
        case ACPI_MADT_IOAPIC:
        case ACPI_MADT_ISO:
        case ACPI_MADT_NMI:
        case ACPI_MADT_LAPIC64:
            break;
        default:
            cprintf("unknown entry\n");
            ismp = 0;
            break;
        }
    }

    bootcpu->cpu_status = CPU_STARTED;

    if (!ismp) {
        /* Didn't like what we found; fall back to no MP. */
        ncpu = 1;
        lapicaddr = 0;
        cprintf("SMP: configuration not found, SMP disabled\n");
        return;
    }

    cprintf("SMP: CPU %d found %d CPU(s)\n", bootcpu->cpu_id,  ncpu);

    if (madt->flags) {
        /*
         * Check if the hardware implements PIC mode, and switch to getting
         * interrupts from the local APIC if it does.
         */
        cprintf("SMP: Setting IMCR to switch from PIC mode to symmetric I/O mode\n");
        outb(0x22, 0x70);   /* Select IMCR */
        outb(0x23, inb(0x23) | 1);  /* Mask external interrupts. */
    }
}
