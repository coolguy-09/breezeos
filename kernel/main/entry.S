.intel_syntax noprefix
.global _start
.extern kmain

_start:
    cli
    mov rsp, stack_top
    call kmain
.hang:
    hlt
    jmp .hang

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
