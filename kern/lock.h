#include <kern/spinlock.h>

// Lab 6
int lock_page_unlock_env();
void unlock_page_lock_env(int lock);
int pmap_unlock_env();
void pmap_lock_env(int lock);
int env_lock_env();
void env_unlock_env(int kern);

// Lab 7
int swap_lock_pagealloc();
void swap_unlock_pagealloc(int kern);
