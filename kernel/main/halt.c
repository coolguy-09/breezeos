#include <halt.h>

void halt(void) { for (;;) asm("hlt"); }
