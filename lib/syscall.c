/* System call stubs. */

#include <inc/syscall.h>
#include <inc/lib.h>

extern int64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5);

static inline unsigned long syscall(int num, int check,
    unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4,
    unsigned long a5)
{
    unsigned long ret;

    /*
     * Generic system call: pass system call number in AX,
     * up to five parameters in DX, CX, BX, DI, SI.
     * Interrupt kernel with T_SYSCALL.
     *
     * The "volatile" tells the assembler not to optimize
     * this instruction away just because we don't use the
     * return value.
     *
     * The last clause tells the assembler that this can
     * potentially change the condition codes and arbitrary
     * memory locations.
     */
    ret = do_syscall(num, a1, a2, a3, a4, a5);

    if(check && ret < 0)
        panic("syscall %d returned %d (> 0)", num, ret);

    return ret;
}

void sys_cputs(const char *s, size_t len)
{
    syscall(SYS_cputs, 0, (uintptr_t)s, len, 0, 0, 0);
}

int sys_cgetc(void)
{
    return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0);
}

int sys_env_destroy(envid_t envid)
{
    return syscall(SYS_env_destroy, 1, envid, 0, 0, 0, 0);
}

envid_t sys_getenvid(void)
{
     return syscall(SYS_getenvid, 0, 0, 0, 0, 0, 0);
}

void *sys_vma_create(size_t size, int perm, int flags)
{
    /* LAB 4: Your code here */
    return (void *) syscall(SYS_vma_create, 1, size, perm, flags, 0, 0);
}

int sys_vma_destroy(void *va, size_t size)
{
    /* LAB 4: Your code here */
    return syscall(SYS_vma_destroy, 1, (unsigned long) va, size, 0, 0, 0);
}
