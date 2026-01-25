#include <stdint.h>
#include <terminal.h>

void syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2) {
    switch (syscall_num) {
        case 1:
            printf("Hello world from syscall!");
            break;
        default:
            printf("Unknown syscall: %llu\n", syscall_num);
            break;
    }
}