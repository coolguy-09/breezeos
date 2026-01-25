#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <limine.h>
#include <framebuffer.h>

void putc(char c);
void puts(const char *str);
void scroll(struct limine_framebuffer *fb);
void clrscr(void);
void printf(const char *fmt, ...);