.intel_syntax noprefix

.global timer_handler
.extern current_task
.extern other_task

.section .text

timer_handler:
    // 1. Save EVERYTHING immediately
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    // 1. Save current state
    mov rax, [current_task]
    mov [rax], rsp

    // 2. Swap the pointers
    mov rbx, [other_task]
    mov [other_task], rax
    mov [current_task], rbx

    // 3. Load the new state
    mov rsp, [rbx]

    // 2. Send EOI now while we know rax is safe to use 
    // (since we already pushed it)
    mov al, 0x20
    out 0xa0, al
    out 0x20, al

    // 5. Restore
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq