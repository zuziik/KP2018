/*
 * Minimal PIO-based (non-interrupt-driven) IDE driver code.
 */

#include <inc/x86-64/asm.h>
#include <inc/assert.h>
#include <kern/ide.h>


/*
 * I/O registers used to communicate with primary disk controller (primary
 * master/slave disks).
 */
#define IDE_REG_BASE        0x1F0
#define IDE_REG_DATA        (IDE_REG_BASE + 0x0)
#define IDE_REG_SECCNT      (IDE_REG_BASE + 0x2)
#define IDE_REG_LBA_LO      (IDE_REG_BASE + 0x3)
#define IDE_REG_LBA_MID     (IDE_REG_BASE + 0x4)
#define IDE_REG_LBA_HI      (IDE_REG_BASE + 0x5)
#define IDE_REG_DRIVE       (IDE_REG_BASE + 0x6)
#define IDE_REG_COMMAND     (IDE_REG_BASE + 0x7) /* when writing */
#define IDE_REG_STATUS      (IDE_REG_BASE + 0x7) /* when reading */

/* When reading IDE_REG_STATUS */
#define IDE_BSY             0x80
#define IDE_RDY             0x40
#define IDE_DF              0x20
#define IDE_ERR             0x01

/* When writing IDE_REG_COMMAND */
#define IDE_COM_READ        0x20
#define IDE_COM_WRITE       0x30
#define IDE_COM_IDENTIFY    0xEC

/* What disk to use. 0=master, 1=slave */
static int diskno = 1;
/* Number of sectors on our disk, initialized by ide_identify. */
static uint32_t disk_sectors;

int ide_is_ready(void)
{
    return (inb(IDE_REG_STATUS) & (IDE_BSY|IDE_RDY)) == IDE_RDY;
}

static int ide_wait_ready(bool check_error)
{
    int r;

    while (((r = inb(IDE_REG_STATUS)) & (IDE_BSY|IDE_RDY)) != IDE_RDY)
        /* do nothing */;

    if (check_error && (r & (IDE_DF|IDE_ERR)) != 0)
        return -1;
    return 0;
}

static bool ide_probe_disk1(void)
{
    int r, x;

    /* Wait for device 0 to be ready. */
    ide_wait_ready(0);

    /* Switch to device 1. */
    outb(IDE_REG_DRIVE, 0xE0 | (1<<4));

    /* Check for device 1 to be ready for a while. */
    for (x = 0;
         x < 1000 &&
            ((r = inb(IDE_REG_STATUS)) & (IDE_BSY|IDE_DF|IDE_ERR)) != 0;
         x++)
        /* do nothing */;

    /* Switch back to device 0 */
    outb(IDE_REG_DRIVE, 0xE0 | (0<<4));

    cprintf("[IDE] Device 1 presence: %d\n", (x < 1000));
    return x < 1000;
}

static void ide_set_disk(int d)
{
    if (d != 0 && d != 1)
        panic("bad disk number");
    diskno = d;
}

void ide_start_readwrite(uint32_t secno, size_t nsectors, int iswrite)
{
    int r;

    assert(nsectors <= 256);

    ide_wait_ready(0);

    outb(IDE_REG_SECCNT,  nsectors);
    outb(IDE_REG_LBA_LO,   secno        & 0xFF);
    outb(IDE_REG_LBA_MID, (secno >>  8) & 0xFF);
    outb(IDE_REG_LBA_HI,  (secno >> 16) & 0xFF);
    outb(IDE_REG_DRIVE,   0xE0 | ((diskno&1)<<4) | ((secno>>24)&0x0F));
    outb(IDE_REG_COMMAND, iswrite ? IDE_COM_WRITE : IDE_COM_READ);
}


void ide_read_sector(char *dst)
{
    insl(IDE_REG_BASE, dst, SECTSIZE / 4);
}


void ide_write_sector(char *src)
{
    outsl(IDE_REG_BASE, src, SECTSIZE / 4);
}

/*
 * Retrieve information about disk (such as number of sectors) using IDENTIFY
 * command.
 */
static void ide_identify(void)
{
    uint16_t info[256];

    ide_wait_ready(0);

    outb(IDE_REG_SECCNT,  0);
    outb(IDE_REG_LBA_LO,  0);
    outb(IDE_REG_LBA_MID, 0);
    outb(IDE_REG_LBA_HI,  0);
    outb(IDE_REG_DRIVE,   0xA0 | ((diskno&1)<<4));
    outb(IDE_REG_COMMAND, IDE_COM_IDENTIFY);

    if (ide_wait_ready(1))
        panic("Error during IDENTIFY of IDE disk");

    ide_read_sector((char*)info);
    disk_sectors = *((uint32_t*)&info[60]);
    cprintf("[IDE] Found %u sectors on disk (=%dM)\n", disk_sectors,
            disk_sectors / 2 / 1024);
}

uint32_t ide_num_sectors(void)
{
    return disk_sectors;
}

int ide_init(void)
{
    if (!ide_probe_disk1())
        panic("IDE: Could not find disk 1!");
    ide_set_disk(1);
    ide_identify();
    return 0;
}
