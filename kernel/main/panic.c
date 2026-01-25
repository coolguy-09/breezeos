#include <terminal.h>
#include <panic.h>
#include <halt.h>

void panic(const char *reason) {
	printf("Kernel panic: %s\n", reason);
	halt();
}