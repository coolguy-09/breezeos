void sys_helloworld(void) {
    asm volatile (
        "mov $1, %%rax\n"   // Syscall number 1
        "syscall\n"
        :                   // No outputs
        :                   // No inputs
        : "rax", "rcx", "r11" // "Clobber list": tells GCC these registers will change!
    );
}

void _start() {
    sys_helloworld();
    while(1);
}
