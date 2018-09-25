#include <kern/env.h>
#include <inc/env.h>
#include <kern/pmap.h>

struct vma *vma_lookup(struct env *env, void *va);
struct vma *vma_insert(struct env *env, int type, void *va, size_t len,
    int perm, void *binary_start_user, void *binary_start_kernel, uint64_t binary_size);
uintptr_t vma_get_vmem(size_t size, struct vma *vma);
void vma_map_populate(uintptr_t va, size_t size, int perm, struct env *env);
void vma_unmap(uintptr_t va, size_t size, struct env *env);
struct vma *vma_get_last(struct vma *vma);
void vma_make_unused(struct env *env, struct vma *vma);