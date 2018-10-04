/* Buggy program - faults with a write to a kernel location. */

#include <inc/lib.h>

void umain(int argc, char **argv)
{
    *(unsigned*)0xffff800000100000 = 0;
}

