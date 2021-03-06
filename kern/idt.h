#pragma once

#include <inc/x86-64/idt.h>

void iret64(struct int_frame *frame);

void print_int_frame(struct int_frame *frame);
void idt_init(void);
void idt_init_percpu(void);
void int_handler(struct int_frame *frame);
void page_fault_handler(struct int_frame *frame);
int page_fault_load_page(void *fault_va_aligned);