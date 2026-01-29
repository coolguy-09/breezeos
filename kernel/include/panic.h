#pragma once

void panic(const char *reason);
void exception_panic(uint64_t vector, uint64_t rip, uint64_t rsp);