#include <stdint.h>
#include <string.h>
#include <terminal.h>
#include <panic.h>
#include <rootfs.h>
#include <mm.h>
#include <halt.h>
#include <io.h>
#include <syscall.h>
#include <idt.h>
#include <stddef.h>
#include <stdbool.h>

extern volatile struct limine_module_request mod_req;
extern volatile struct limine_executable_file_request cmdline_req;
extern volatile struct limine_hhdm_request hhdm_req;
extern volatile struct limine_memmap_request mm_req;

uint64_t hhdm_offset = 0;
static int program_count = 0;

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

#define MAX_PROCS 32

void run_bin(void *entry, void *stack_top, void *handler) {
    static int program_count = 0;
    program_count++;
    if (program_count > MAX_PROCS) {
        panic("Too many programs running");
    }

    // build array of return labels
    void *ret_addrs[MAX_PROCS] = {
        &&kernel_return_1, &&kernel_return_2, &&kernel_return_3, &&kernel_return_4,
        &&kernel_return_5, &&kernel_return_6, &&kernel_return_7, &&kernel_return_8,
        &&kernel_return_9, &&kernel_return_10, &&kernel_return_11, &&kernel_return_12,
        &&kernel_return_13, &&kernel_return_14, &&kernel_return_15, &&kernel_return_16,
        &&kernel_return_17, &&kernel_return_18, &&kernel_return_19, &&kernel_return_20,
        &&kernel_return_21, &&kernel_return_22, &&kernel_return_23, &&kernel_return_24,
        &&kernel_return_25, &&kernel_return_26, &&kernel_return_27, &&kernel_return_28,
        &&kernel_return_29, &&kernel_return_30, &&kernel_return_31, &&kernel_return_32
    };

    void *ret_addr = ret_addrs[program_count - 1];
    void (*bin_entry)(void) = (void (*)(void)) entry;

    asm volatile (
        "mov %0, %%rsp\n"
        "push %1\n"
        "call *%2\n"
        :
        : "r"(stack_top), "r"(ret_addr), "r"(bin_entry)
        : "memory"
    );

    // 32 slots, all funnel into handler
kernel_return_1:  goto *handler;
kernel_return_2:  goto *handler;
kernel_return_3:  goto *handler;
kernel_return_4:  goto *handler;
kernel_return_5:  goto *handler;
kernel_return_6:  goto *handler;
kernel_return_7:  goto *handler;
kernel_return_8:  goto *handler;
kernel_return_9:  goto *handler;
kernel_return_10: goto *handler;
kernel_return_11: goto *handler;
kernel_return_12: goto *handler;
kernel_return_13: goto *handler;
kernel_return_14: goto *handler;
kernel_return_15: goto *handler;
kernel_return_16: goto *handler;
kernel_return_17: goto *handler;
kernel_return_18: goto *handler;
kernel_return_19: goto *handler;
kernel_return_20: goto *handler;
kernel_return_21: goto *handler;
kernel_return_22: goto *handler;
kernel_return_23: goto *handler;
kernel_return_24: goto *handler;
kernel_return_25: goto *handler;
kernel_return_26: goto *handler;
kernel_return_27: goto *handler;
kernel_return_28: goto *handler;
kernel_return_29: goto *handler;
kernel_return_30: goto *handler;
kernel_return_31: goto *handler;
kernel_return_32: goto *handler;
}

void remap_pic(void) {
    // 1. Save the current masks (what's currently enabled/disabled)
    // Data ports are 0x21 and 0xA1
    uint8_t m1 = inb(0x21);
    uint8_t m2 = inb(0xA1);

    // 2. Start initialization (ICW1)
    // Command ports are 0x20 and 0xA0
    outb(0x20, 0x11); 
    outb(0xA0, 0x11);

    // 3. Set Vector Offsets (ICW2)
    outb(0x21, 0x20); // Master starts at 32
    outb(0xA1, 0x28); // Slave starts at 40

    // 4. Cascading (ICW3)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    // 5. Environment Mode (ICW4)
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // 6. Restore the masks we saved at the start
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

#define STACK_SIZE 8192
#define MAX_TASKS 128

typedef struct {
    uint64_t rsp;
    bool is_active;
    // Pre-allocate the stack right here in the struct
    uint8_t stack_area[STACK_SIZE] __attribute__((aligned(16)));
} tcb_t;

tcb_t task_list[MAX_TASKS];
int current_task_index = 0;
int task_count = 0;
bool scheduler_enabled = false;

uint64_t schedule(uint64_t current_rsp) {
    // Safety check: if multitasking isn't ready, don't switch!
    if (!scheduler_enabled) return current_rsp;

    // Save the RSP of the task that was just interrupted
    task_list[current_task_index].rsp = current_rsp;

    // Move to the next active task
    current_task_index = (current_task_index + 1) % task_count;

    //printf("%d", current_task_index);

    // Return the RSP of the next task
    return task_list[current_task_index].rsp;
}

void init_pit(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;

    // 0x36 sets the PIT to Square Wave Mode and expects 2 bytes for the divisor
    outb(0x43, 0x36);

    // Send the frequency divisor (Split into low/high bytes)
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void wulzy_task() {
    while(1) {
        printf("Woah its wulzy! ");
        for(volatile int i = 0; i < 100000000; i++);
    }
}

void create_task(void* entry_point) {
    if (task_count >= MAX_TASKS) return;

    // Start at the very top of the pre-allocated stack area
    uint64_t stack_raw = (uint64_t)&task_list[task_count].stack_area[STACK_SIZE];
    uint64_t stack_top = stack_raw & -16LL; 
    uint64_t* stack = (uint64_t*)stack_top;

    // 1. IRETQ Frame (5 items)
    *(--stack) = 0x30;              // SS
    *(--stack) = stack_top;         // RSP
    *(--stack) = 0x202;             // RFLAGS (Interrupts enabled)
    *(--stack) = 0x28;              // CS
    *(--stack) = (uint64_t)entry_point; // RIP

    // 2. General Purpose Registers (15 items)
    // Matches the 15 'pop' instructions in isr32
    for (int i = 0; i < 15; i++) {
        *(--stack) = 0;
    }

    task_list[task_count].rsp = (uint64_t)stack;
    task_list[task_count].is_active = true;
    task_count++;
}

void kmain(void) {
    clrscr();
    remap_pic();
    init_idt();
    init_heap();
    init_syscall();
    init_rootfs();

    /*rootfs_file_t file = read_rootfs("./test.bin");
    void (*entry)(void) = (void(*)(void)) file.data;

    uint8_t *stack = malloc(16*1024);
    uint8_t *stack_top = stack + 16*1024;

    // pass address of handler
    run_bin(entry, stack_top, &&kernel_return_handler);

kernel_return_handler:
    printf("Returned to kernel handler\n");
    */
    task_list[0].is_active = true; 
    task_count = 1;                // We now have 1 task (kmain)
    current_task_index = 0;        // We are currently running Task 0

    // 2. CREATE TASK B (Wulzy)
    // This will now go into task_list[1]
    create_task(wulzy_task);

    scheduler_enabled = true; 
    init_pit(100);    
    
    // 3. START MULTITASKING
    asm volatile("sti"); 
    
    // 4. THIS LOOP IS NOW "TASK 0"
    while(1) {
        printf("K "); 
        // Delay loop so we don't flood the screen
        for(volatile int i = 0; i < 100000000; i++); 
    }
}
