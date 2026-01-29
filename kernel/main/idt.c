#include <idt.h>
#include <terminal.h>
#include <panic.h>
#include <halt.h>

__attribute__((aligned(0x10))) 
static struct idt_entry idt[256];
static struct idt_ptr idtr;

// Your existing exception handlers
extern void isr13(void);
extern void isr14(void);
// Your new Timer/Multitasking handler
extern void isr32(void);

// This is where we'll switch tasks later
extern uint64_t schedule(uint64_t current_rsp);

__asm__(
    ".align 8\n"

    // --- EXCEPTIONS (Vectors 13, 14) ---
    "isr13: pushq $0; pushq $13; jmp isr_common\n"
    "isr14:            pushq $14; jmp isr_common\n"

    "isr_common:\n"
    "    push %rdi; push %rsi; push %rdx; push %rcx\n"
    "    push %r8;  push %r9;  push %r10; push %r11\n"
    "    mov 64(%rsp), %rdi\n"   // vector
    "    mov 80(%rsp), %rsi\n"   // RIP
    "    mov 104(%rsp), %rdx\n"  // RSP
    "    call exception_panic\n"

    // --- TIMER IRQ (Vector 32) ---
    ".global isr32\n"
    "isr32:\n"
    "    /* CPU already pushed RIP, CS, RFLAGS */\n"

    "    //pushq $32\n"
    "    //pushq $0\n"

    "    push %rax\n"
    "    push %rbx\n"
    "    push %rcx\n"
    "    push %rdx\n"
    "    push %rsi\n"
    "    push %rdi\n"
    "    push %rbp\n"
    "    push %r8\n"
    "    push %r9\n"
    "    push %r10\n"
    "    push %r11\n"
    "    push %r12\n"
    "    push %r13\n"
    "    push %r14\n"
    "    push %r15\n"

    "    mov %rsp, %rdi\n"
    "    mov %rsp, %r12\n"
    "    and $-16, %rsp\n"
    "    call schedule\n"
    "    mov %rax, %rsp\n"

    "    movb $0x20, %al\n"
    "    outb %al, $0x20\n"

    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %r11\n"
    "    pop %r10\n"
    "    pop %r9\n"
    "    pop %r8\n"
    "    pop %rbp\n"
    "    pop %rdi\n"
    "    pop %rsi\n"
    "    pop %rdx\n"
    "    pop %rcx\n"
    "    pop %rbx\n"
    "    pop %rax\n"

    "    //add $16, %rsp\n"        // pop error + vector
    "    iretq\n"
);


void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    struct idt_entry* descriptor = &idt[vector];
    descriptor->isr_low    = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs  = 0x28; // Limine 64-bit CS
    descriptor->ist        = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid    = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high   = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

void init_idt() {
    idtr.base = (uint64_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(struct idt_entry) * 256 - 1;

    // Exceptions (Panic)
    idt_set_descriptor(13, isr13, 0x8E);
    idt_set_descriptor(14, isr14, 0x8E);

    // Hardware Interrupt (Timer - Multitasking)
    idt_set_descriptor(32, isr32, 0x8E);

    asm volatile("lidt %0" : : "m"(idtr));
}