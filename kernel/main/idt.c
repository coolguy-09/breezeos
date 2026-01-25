#include <stdint.h>

struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
extern void simple_trap(void);
extern void timer_handler(void);

void set_idt_gate(int vector, uint64_t handler_addr) {
    idt[vector].isr_low   = (uint16_t)handler_addr;
    idt[vector].kernel_cs = 0x28; // Change from 0x08 to 0x28
    idt[vector].ist       = 0;
    idt[vector].attributes = 0x8E; 
    idt[vector].isr_mid   = (uint16_t)(handler_addr >> 16);
    idt[vector].isr_high  = (uint32_t)(handler_addr >> 32);
    idt[vector].reserved  = 0;
}

void init_idt(void) {
    uint64_t handler = (uint64_t)simple_trap;

    for (int i = 0; i < 256; i++) {
        idt[i].isr_low = (uint16_t)handler;
        idt[i].kernel_cs = 0x08; // Limine's default code selector
        idt[i].ist = 0;
        idt[i].attributes = 0x8E; // Present, Ring 0, Interrupt Gate
        idt[i].isr_mid = (uint16_t)(handler >> 16);
        idt[i].isr_high = (uint32_t)(handler >> 32);
        idt[i].reserved = 0;
    }

    for(int i = 0; i < 256; i++) set_idt_gate(i, (uint64_t)simple_trap);
    set_idt_gate(32, (uint64_t)timer_handler);

    struct idt_ptr idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    asm volatile("lidt %0" : : "m"(idtr));
}