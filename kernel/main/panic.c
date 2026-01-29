#include <terminal.h>
#include <panic.h>
#include <halt.h>

void panic(const char *reason) {
	uint64_t rip = (uint64_t)__builtin_return_address(0);
	uint64_t rsp;
        asm volatile("mov %%rsp, %0" : "=r"(rsp));
	printf("\nKernel panic: %s\n", reason);
	printf("\nRegisters:\n");
	printf(" RIP: 0x%llX\n", rip);
	printf(" RSP: 0x%llX\n", rsp);
	halt();
}

void exception_panic(uint64_t vector, uint64_t rip, uint64_t rsp) {
	printf("\nKernel panic: ");
	if (vector == 13) printf("A general protection fault occurred.\n");
	else if (vector == 14) printf("A page fault occurred.\n");
	else printf("An unknown exception occurred.\n");
	printf("\nRegisters:\n");
	printf(" RIP: 0x%llX\n", rip);
	printf(" RSP: 0x%llX\n", rsp);
	halt();
}