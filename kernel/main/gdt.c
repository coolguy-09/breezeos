#include <stdint.h>
#include <gdt.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry gdt[5]; // null, kernel code/data, user code/data
struct gdt_ptr gp;

void init_gdt(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;

    // Null
    gdt[0] = (struct gdt_entry){0};

    // Kernel code
    gdt[1] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_middle = 0,
        .access = 0x9A, // present, ring0, executable, readable
        .granularity = 0x20, // 64-bit
        .base_high = 0
    };

    // Kernel data
    gdt[2] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_middle = 0,
        .access = 0x92, // present, ring0, writable
        .granularity = 0x00,
        .base_high = 0
    };

    // User code
    gdt[3] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_middle = 0,
        .access = 0xFA, // present, ring3, executable
        .granularity = 0x20,
        .base_high = 0
    };

    // User data
    gdt[4] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_middle = 0,
        .access = 0xF2, // present, ring3, writable
        .granularity = 0x00,
        .base_high = 0
    };

    asm volatile("lgdt %0" : : "m"(gp));

    // reload segments
    asm volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%ss\n"
        :
        :
        : "ax"
    );
}
