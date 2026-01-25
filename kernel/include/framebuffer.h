#pragma once

#include <stdint.h>
#include <limine.h>

extern unsigned char font8x16[][16];
extern volatile struct limine_framebuffer_request fb_req;

void fbputc(struct limine_framebuffer *fb, char c, int x, int y, uint32_t fg, uint32_t bg);