#pragma once

#include <inc/x86-64/idt.h>

void iret64(struct int_frame *frame);

void print_int_frame(struct int_frame *frame);
void idt_init(void);
void idt_init_percpu(void);
void int_handler(struct int_frame *frame);
void page_fault_handler(struct int_frame *frame);

// A protection violation was discovered: check if COW must be used
int cow(void *fault_va, uintptr_t fault_va_aligned, int is_write);

// Do on demand paging after a page fault has occured
int page_fault_load_page(void *fault_va_aligned);