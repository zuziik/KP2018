#pragma once

struct acpi_rsdp {
    char sig[8];
    uint8_t chksum;
    char oem[6];
    uint8_t rev;
    uint32_t rsdt_base;
} __attribute__ ((packed));

struct acpi_rsdp2 {
    struct acpi_rsdp base;
    uint32_t len;
    uint64_t xsdt_base;
    uint8_t chksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

struct acpi_hdr {
    char sig[4];
    uint32_t len;
    uint8_t rev;
    uint8_t chksum;
    char oem[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__ ((packed));

struct acpi_madt {
    struct acpi_hdr hdr;
    uint32_t lapic_base;
    uint32_t flags;
} __attribute__ ((packed));

struct acpi_lapic {
    uint8_t cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__ ((packed));

struct acpi_ioapic {
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t ioapic_base;
    uint32_t gsi_base;
} __attribute__ ((packed));

struct acpi_iso {
    uint8_t bus_src;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __attribute__ ((packed));

struct acpi_nmi {
    uint8_t cpu_id;
    uint16_t flags;
    uint8_t lint;
} __attribute__ ((packed));

struct acpi_lapic64 {
    uint16_t reserved;
    uint64_t lapic_base;
} __attribute__ ((packed));

struct acpi_madt_entry {
    uint8_t type;
    uint8_t len;
    union {
        struct acpi_lapic lapic;
        struct acpi_ioapic ioapic;
        struct acpi_iso iso;
        struct acpi_nmi nmi;
        struct acpi_lapic64 lapic64;
    };
} __attribute__ ((packed));

#define ACPI_MADT_LAPIC   0
#define ACPI_MADT_IOAPIC  1
#define ACPI_MADT_ISO     2
#define ACPI_MADT_NMI     4
#define ACPI_MADT_LAPIC64 5

int acpi_validate(struct acpi_hdr *hdr, const char *sig);
struct acpi_rsdp *acpi_find_rsdp(void);
void *acpi_find_table(struct acpi_rsdp *rsdp, const char *sig);
