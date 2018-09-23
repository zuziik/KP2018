#include <kern/env.h>
#include <inc/env.h>

struct vma *vma_lookup(struct env *e, void *va);
int vma_insert(struct vma *new_vma, struct env *env);
int vma_get_vmem(size_t size, struct vma *vma);
void vma_map_populate(uintptr_t va, size_t size, int perm, struct env *env);
void vma_unmap(uintptr_t va, size_t size, struct env *env);
