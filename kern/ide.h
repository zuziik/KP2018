#ifndef JOS_KERN_IDE_H
#define JOS_KERN_IDE_H

#include <inc/types.h>

#define SECTSIZE 512

/*
 * Initialized the IDE driver. For JOS, we will only use this driver for
 * swapping, which is done on a separate disk: the slave of the pimary
 * controller. Thus, initialization will check if this disk is present, and
 * panic if it is not. It will then switch over to use this disk for all future
 * operations of this drivers, and read the number of sectors on this disk so
 * you can access that information later.
 */
int ide_init(void);
uint32_t ide_num_sectors(void);

/*
 * Polls the disk and returns whether it is ready to do a read/write of a
 * sector.
 */
int ide_is_ready(void);

/*
 * Starts a read or write operation to the disk for a certain number of
 * sectors. After initiating an operation, you should check whether the disk is
 * ready, and then read/write a single sector using the other functions here,
 * repeating until all sectors have been accessed.
 * Note: make sure to finish an operation entirely before starting a new one,
 * the driver itself is not concurrency-safe.
 */
void ide_start_readwrite(uint32_t secno, size_t nsectors, int iswrite);
static inline void ide_start_read(uint32_t secno, size_t nsectors)
{
    ide_start_readwrite(secno, nsectors, 0);
}
static inline void ide_start_write(uint32_t secno, size_t nsectors)
{
    ide_start_readwrite(secno, nsectors, 1);
}

/*
 * Read/write an individual sector, once you have started the operation using
 * ide_start_read/write. Also make sure that the disk reported being ready
 * before reading/writing!
 */
void ide_read_sector(char *dst);
void ide_write_sector(char *src);

#endif
