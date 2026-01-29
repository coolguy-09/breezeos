#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <framebuffer.h>
#include <string.h>

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
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    for (uint64_t y = 0; y < fb->height; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb->address + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++) row[x] = g_bg_color;
    }
    g_cursor_x = 0; g_cursor_y = 0;
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

void putc(char c) {
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

void puts(const char *str) {
    while (*str) putc(*str++);
}

static void int_to_str(uint64_t value, char *buf, size_t buf_size, int base, bool uppercase) {
    char temp[64];
    int i = 0;

    if (value == 0) {
        if (buf_size > 1) { buf[0] = '0'; buf[1] = '\0'; }
        return;
    }

    // Determine the letter offset: 'A' (65) for uppercase, 'a' (97) for lowercase
    char hex_offset = uppercase ? 'A' : 'a';

    while (value > 0 && i < 63) {
        uint64_t rem = value % base;
        // If rem is 10, (10 - 10 + 'A') = 'A'. Perfect.
        temp[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + hex_offset);
        value /= base;
    }

    int j = 0;
    while (i > 0 && j < (int)buf_size - 1) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putc(*p);
            continue;
        }

        p++;
        int width = 0;
        char pad_char = ' ';
        bool is_long = false;

        if (*p == '0') {
            pad_char = '0';
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        // Length modifier logic
        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') p++;
        }

        switch (*p) {
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                while(*s) putc(*s++);
                break;
            }

            case 'd': case 'D':
            case 'u': case 'U':
            case 'x': case 'X': {
                uint64_t val;
                // Force long for capital D/U
                bool forced_long = (*p == 'D' || *p == 'U');
                
                if (is_long || forced_long) val = va_arg(args, uint64_t);
                else val = (*p == 'd') ? (uint64_t)va_arg(args, int) : (uint64_t)va_arg(args, unsigned int);

                // Handle signed negative
                if ((*p == 'd' || *p == 'D') && (int64_t)val < 0) {
                    putc('-');
                    val = -(int64_t)val;
                }

                char buf[64];
                int base = (*p == 'x' || *p == 'X') ? 16 : 10;
                // Pass true for uppercase if format is 'X'
                int_to_str(val, buf, 64, base, (*p == 'X'));

                int len = 0;
                while (buf[len]) len++;
                while (width > len) {
                    putc(pad_char);
                    width--;
                }

                char *ptr = buf;
                while(*ptr) putc(*ptr++);
                break;
            }

            case 'p': {
                uint64_t x = va_arg(args, uint64_t);
                char buf[64];
                // Pointers usually use lowercase by convention
                int_to_str(x, buf, 64, 16, false);
                putc('0'); putc('x');
                
                int len = 0;
                while (buf[len]) len++;
                for (int i = 0; i < (16 - len); i++) putc('0');

                char *ptr = buf;
                while(*ptr) putc(*ptr++);
                break;
            }

            case 'c':
                putc((char)va_arg(args, int));
                break;

            case '%':
                putc('%');
                break;

            default:
                putc('%');
                putc(*p);
                break;
        }
    }
    va_end(args);
}