#include <halt.h>

void halt(void) { asm volatile("cli"); for (;;) asm volatile("hlt"); }
