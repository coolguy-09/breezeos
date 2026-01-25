#include <stdint.h>
#include <string.h>
#include <terminal.h>
#include <panic.h>
#include <rootfs.h>
#include <mm.h>
#include <halt.h>
#include <elf.h>
#include <syscall.h>
#include <idt.h>
#include <gdt.h>
#include <io.h>

extern volatile struct limine_module_request mod_req;
extern volatile struct limine_executable_file_request cmdline_req;
extern volatile struct limine_hhdm_request hhdm_req;
extern volatile struct limine_memmap_request mm_req;

uint64_t hhdm_offset = 0;

task_t* current_task;
task_t* other_task;

// ELF stack
uint8_t elf_stack[8192] __attribute__((aligned(16)));

const char *get_boot_args(void) {
    // 1. Check if response exists
    if (cmdline_req.response == NULL || cmdline_req.response->executable_file == NULL) {
        return NULL;
    }

    // 2. Use 'string' as we saw in your grep of struct limine_file
    char *args = cmdline_req.response->executable_file->string;

    if (args && *args) {
        return args;
    } else {
        return NULL;
    }
}

void init_heap(void) {
    // 1. Get the offset for Virtual Memory
    if (hhdm_req.response == NULL) panic("Didn't get hhdm response?"); // Should not happen
    hhdm_offset = hhdm_req.response->offset;

    // 2. Look for usable memory in your mm_req
    struct limine_memmap_response *memmap = mm_req.response;
    void* heap_start = NULL;
    size_t heap_len = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        // We want 'USABLE' memory. 
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            // We'll take the first big chunk we find (e.g. > 16MB)
            if (entry->length >= (16 * 1024 * 1024)) {
                // IMPORTANT: Add the offset to make it a virtual address
                heap_start = (void*)(entry->base + hhdm_offset);
                heap_len = entry->length;
                break;
            }
        }
    }

    // 3. Initialize your memory manager
    if (heap_start != NULL) {
        init_mm(heap_start, heap_len);
    } else {
        // No usable memory found? Emergency halt.
        panic("No usable memory found for memory management");
    }
}

void pic_remap(void) {
    // Restart both PICs
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // Offset: Master PIC starts at 32 (0x20), Slave at 40 (0x28)
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    // Tell them how they are connected
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    // Set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    // Unmask Timer (IRQ0) only - 11111110
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}

void init_pit(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;

    // Command: Channel 0, access lo/hi byte, square wave, binary
    outb(0x43, 0x36);

    // Set divisor
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void unmask_timer_interrupt() {
    // Read the current mask from the Master PIC (Port 0x21)
    uint8_t mask = inb(0x21);

    // Bit 0 is IRQ0 (the timer). 0 = Enabled, 1 = Masked.
    mask &= ~(1 << 0);

    // Write the new mask back
    outb(0x21, mask);
}

void kmain(void) {
    clrscr();
    init_gdt();
    init_idt();
    pic_remap();
    init_heap();
    init_syscall();
    init_rootfs();

    rootfs_file_t file = read_rootfs("./test.elf");

    current_task = malloc(sizeof(task_t));
    other_task = create_task(get_address_elf(file));

    // 2. Setup Timer (100Hz)
    init_pit(100);

    // 3. Enable the interrupt
    unmask_timer_interrupt();
    
    printf("Starting multitasking...\n");

    printf("Current: %p, Other: %p\n", current_task, other_task);

    asm volatile("sti"); // Force interrupts back on

    // This code now shares CPU time with your ELF!
    while(1) {
        printf("Kernel Foreground Loop\n");
        for(volatile int i=0; i<50000000; i++); 
    }
}
