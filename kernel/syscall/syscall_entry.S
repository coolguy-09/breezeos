.intel_syntax noprefix
.extern syscall_handler
.global syscall_entry

syscall_entry:
    // 1. Save ELF State
    push r11       // RFLAGS
    push rcx       // Return RIP
    push rbp
    push rdi
    push rsi
    push rdx
    push rbx
    push r12
    push r13
    push r14
    push r15

    // 2. Call C Handler
    mov rdi, rax   // Syscall number
    mov rbp, rsp
    and rsp, -16
    call syscall_handler
    mov rsp, rbp

    // 3. Restore Everything
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop rcx       // Restore Return RIP
    pop r11       // Restore RFLAGS

    // 4. Return to ELF
    jmp rcx       // Jump to the address stored in RCX