#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <framebuffer.h>
#include <string.h>

typedef int spinlock_t;
static spinlock_t g_terminal_lock = 0;

static uint64_t lock_terminal() {
    uint64_t rflags;
    asm volatile("pushfq ; pop %0 ; cli" : "=rm"(rflags) : : "memory");
    while (__sync_lock_test_and_set(&g_terminal_lock, 1)) {
        asm volatile("pause");
    }
    return rflags;
}

static void unlock_terminal(uint64_t rflags) {
    __sync_lock_release(&g_terminal_lock);
    asm volatile("push %0 ; popfq" : : "rm"(rflags) : "memory");
}

uint64_t g_cursor_x = 0;
uint64_t g_cursor_y = 0;
uint32_t g_fg_color = 0xAAAAAA; 
uint32_t g_bg_color = 0x000000;
uint32_t g_default_color = 0xAAAAAA;

typedef enum {
    STATE_NORMAL,
    STATE_EXPECT_BRACKET,
    STATE_READ_PARAMS
} parser_state_t;

parser_state_t g_state = STATE_NORMAL;
char g_ansi_buffer[16];
int g_ansi_idx = 0;
bool g_is_bold = false;

static uint32_t ansi_to_hex(int code, bool is_background, bool bold) {
    static const uint32_t colors[] = {
        0x000000, 0xAA0000, 0x00AA00, 0xAA5500,
        0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
        0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
        0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
    };

    if (code >= 30 && code <= 37) {
        int index = code - 30;
        return colors[bold ? index + 8 : index];
    }
    if (code >= 40 && code <= 47) {
        return colors[code - 40];
    }
    if (code >= 90 && code <= 97) {
        return colors[code - 90 + 8];
    }
    return 0xFFFFFF;
}

void scroll(struct limine_framebuffer *fb) {
    uint8_t *fb_addr = (uint8_t *)fb->address;
    uint64_t line_height = 16;
    uint64_t bytes_per_line = line_height * fb->pitch;
    uint64_t total_fb_size = fb->height * fb->pitch;

    memmove(fb_addr, fb_addr + bytes_per_line, total_fb_size - bytes_per_line);

    for (uint64_t y = fb->height - line_height; y < fb->height; y++) {
        uint32_t *row = (uint32_t *)(fb_addr + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++) {
            row[x] = g_bg_color;
        }
    }

    g_cursor_y -= line_height;
}

void clrscr(void) {
    if (!fb_req.response || fb_req.response->framebuffer_count < 1) return;
    uint64_t flags = lock_terminal();
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    for (uint64_t y = 0; y < fb->height; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb->address + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++) row[x] = g_bg_color;
    }
    g_cursor_x = 0; g_cursor_y = 0;
    unlock_terminal(flags);
}

static void update_cursor(bool visible) {
    if (!fb_req.response || fb_req.response->framebuffer_count < 1) return;
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    uint32_t color = visible ? g_fg_color : g_bg_color;

    for (uint64_t y = 0; y < 16; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb->address + (g_cursor_y + y) * fb->pitch);
        for (uint64_t x = 0; x < 8; x++) {
            row[g_cursor_x + x] = color;
        }
    }
}

void putc_unlocked(char c) {
    if (!fb_req.response || fb_req.response->framebuffer_count < 1) return;
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    update_cursor(false);

    if (g_state == STATE_NORMAL) {
        switch (c) {
            case '\033':
                g_state = STATE_EXPECT_BRACKET;
                break;
            case '\r':
                g_cursor_x = 0;
                break;
            case '\n':
                g_cursor_x = 0;
                g_cursor_y += 16;
                break;
            case '\t':
                g_cursor_x = (g_cursor_x / 64 + 1) * 64; 
                break;
            case '\b':
                if (g_cursor_x >= 8) g_cursor_x -= 8;
                break;
            default:
                if (c >= 0x20 && c <= 0x7E) {
                    if (g_cursor_x + 8 > fb->width) {
                        g_cursor_x = 0;
                        g_cursor_y += 16;
                    }
                    fbputc(fb, c, g_cursor_x, g_cursor_y, g_fg_color, g_bg_color);
                    g_cursor_x += 8;
                }
                break;
        }

        while (g_cursor_y + 16 > fb->height) {
            scroll(fb);
        }
    } 
    else if (g_state == STATE_EXPECT_BRACKET) {
        if (c == '[') {
            g_ansi_idx = 0;
            g_state = STATE_READ_PARAMS;
        } else {
            g_state = STATE_NORMAL;
        }
    } 
    else if (g_state == STATE_READ_PARAMS) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            g_ansi_buffer[g_ansi_idx] = '\0';
            
            if (c == 'm') {
                char *ptr = g_ansi_buffer;
                int val = 0;
                bool has_val = false;

                while (1) {
                    if (*ptr >= '0' && *ptr <= '9') {
                        val = (val * 10) + (*ptr - '0');
                        has_val = true;
                    } else if (*ptr == ';' || *ptr == '\0') {
                        if (has_val || (val == 0 && *ptr == '\0' && g_ansi_idx == 0)) {
                            if (val == 0) {
                                g_fg_color = 0xAAAAAA;
                                g_bg_color = 0x000000;
                                g_is_bold = false;
                            } else if (val == 1) {
                                g_is_bold = true;
                            } else if (val >= 30 && val <= 37) {
                                g_fg_color = ansi_to_hex(val, false, g_is_bold);
                            } else if (val >= 40 && val <= 47) {
                                g_bg_color = ansi_to_hex(val, true, false);
                            }
                        }
                        if (*ptr == '\0') break;
                        val = 0; has_val = false;
                    }
                    ptr++;
                }
            }
            g_state = STATE_NORMAL;
        } else {
            if (g_ansi_idx < 15) g_ansi_buffer[g_ansi_idx++] = c;
        }
    }
    update_cursor(true);
}

void putc(char c) {
    uint64_t flags = lock_terminal();
    putc_unlocked(c);
    unlock_terminal(flags);
}

void puts(const char *str) {
    uint64_t flags = lock_terminal();
    while (*str) putc_unlocked(*str++);
    unlock_terminal(flags);
}

static void int_to_str(uint64_t value, char *buf, size_t buf_size, int base) {
    char temp[64];
    int i = 0;
    if (value == 0) {
        if (buf_size > 1) { buf[0] = '0'; buf[1] = '\0'; }
        return;
    }
    while (value > 0 && i < 63) {
        uint64_t rem = value % base;
        temp[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        value /= base;
    }
    int j = 0;
    while (i > 0 && j < (int)buf_size - 1) buf[j++] = temp[--i];
    buf[j] = '\0';
}

void printf(const char *fmt, ...) {
    uint64_t flags = lock_terminal();
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putc_unlocked(*p);
            continue;
        }

        p++;

        int precision = -1;
        bool is_long = false;

        if (*p == '.') {
            p++;
            if (*p == '*') {
                precision = va_arg(args, int);
                p++;
            }
        }

        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') {
                p++;
            }
        }

        switch (*p) {
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                if (precision >= 0) {
                    for (int i = 0; i < precision && s[i] != '\0'; i++) {
                        putc_unlocked(s[i]);
                    }
                } else {
                    char *ptr = s;
                    while(*ptr) putc_unlocked(*ptr++);
                }
                break;
            }

            case 'd': {
                int64_t d;
                if (is_long) d = va_arg(args, int64_t);
                else d = va_arg(args, int);
                char buf[64];
                if (d < 0) { putc_unlocked('-'); d = -d; }
                int_to_str((uint64_t)d, buf, 64, 10);
                char *ptr = buf;
                while(*ptr) putc_unlocked(*ptr++);
                break;
            }

            case 'u': {
                uint64_t u;
                if (is_long) u = va_arg(args, uint64_t);
                else u = va_arg(args, unsigned int);
                char buf[64];
                int_to_str(u, buf, 64, 10);
                char *ptr = buf;
                while(*ptr) putc_unlocked(*ptr++);
                break;
            }

            case 'x': {
                uint64_t x;
                if (is_long) x = va_arg(args, uint64_t);
                else x = va_arg(args, unsigned int);
                char buf[64];
                int_to_str(x, buf, 64, 16);
                char *ptr = buf;
                while(*ptr) putc_unlocked(*ptr++);
                break;
            }

            case 'p': {
                uint64_t x = va_arg(args, uint64_t);
                char buf[64];
                int_to_str(x, buf, 64, 16);
                putc_unlocked('0');
                putc_unlocked('x');
                char *ptr = buf;
                while(*ptr) putc_unlocked(*ptr++);
                break;
            }

            case 'c':
                putc_unlocked((char)va_arg(args, int));
                break;

            case '%':
                putc_unlocked('%');
                break;

            default:
                putc_unlocked('%');
                putc_unlocked(*p);
                break;
        }
    }
    va_end(args);
    unlock_terminal(flags);
}