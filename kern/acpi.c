#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/acpi.h>

int acpi_validate(struct acpi_hdr *hdr, const char *sig)
{
    char *p;
    size_t i;
    unsigned char sum = 0;

    if (memcmp(hdr->sig, sig, 4) != 0)
        return 0;

    for (i = 0, p = (char *)hdr; i < hdr->len; ++i, ++p) {
        sum += *p;
    }

    return sum == 0;
}

void *acpi_find_rsdp_in_range(void *start, void *end)
{
    void *p;

    for (p = start; p < end; p += 16) {
        if (memcmp(p, "RSD PTR ", 8) == 0)
            return p;
    }

    return NULL;
}

struct acpi_rsdp *acpi_find_rsdp(void)
{
    struct acpi_rsdp *rsdp;

    rsdp = acpi_find_rsdp_in_range((void *)(KERNEL_VMA + 0x000e0000),
        (void *)(KERNEL_VMA + 0x000fffff));

    if (rsdp)
        return rsdp;

    return NULL;
}

static void *xsdt_find_table(struct acpi_hdr *hdr, const char *sig)
{
    uint64_t *p;
    size_t i, nentries;

    nentries = hdr->len / sizeof *p;

    for (i = 0, p = (uint64_t *)(hdr + 1); i < nentries; ++i, ++p) {
        hdr = (struct acpi_hdr *)(KERNEL_VMA + *p);

        if (!acpi_validate(hdr, sig))
            continue;

        return hdr;
    }

    return NULL;
}

static void *rsdt_find_table(struct acpi_hdr *hdr, const char *sig)
{
    uint32_t *p;
    size_t i, nentries;

    nentries = hdr->len / sizeof *p;

    for (i = 0, p = (uint32_t *)(hdr + 1); i < nentries; ++i, ++p) {
        hdr = (struct acpi_hdr *)(KERNEL_VMA + *p);

        if (!acpi_validate(hdr, sig))
            continue;

        return hdr;
    }

    return NULL;
}

void *acpi_find_table(struct acpi_rsdp *rsdp, const char *sig)
{
    struct acpi_rsdp2 *rsdp2;
    struct acpi_hdr *hdr;

    if (rsdp->rev > 0) {
        rsdp2 = (struct acpi_rsdp2 *)rsdp;

        hdr = (struct acpi_hdr *)(KERNEL_VMA + rsdp2->xsdt_base);

        if (acpi_validate(hdr, "XSDT"))
            return xsdt_find_table(hdr, sig);
    }

    hdr = (struct acpi_hdr *)(KERNEL_VMA + rsdp->rsdt_base);

    if (!acpi_validate(hdr, "RSDT"))
        return NULL;

    return rsdt_find_table(hdr, sig);
}
