#include <kern/spinlock.h>

int lock_page_unlock_env();
void unlock_page_lock_env(int lock);
int pmap_unlock_env();
void pmap_lock_env(int lock);
int env_lock_env();
void env_unlock_env(int kern);