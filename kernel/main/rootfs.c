#include <stdint.h>
#include <stddef.h>
#include <panic.h>
#include <terminal.h>
#include <rootfs.h>
#include <string.h>
#include <gzip.h>
#include <mm.h>

extern volatile struct limine_memmap_request mm_req;
extern volatile struct limine_module_request mod_req;
static uint8_t *tar_archive_start = NULL;

// Helper: Convert Octal ASCII string to integer
static uint64_t parse_octal(const char *str) {
    uint64_t val = 0;
    while (*str >= '0' && *str <= '7') {
        val = (val << 3) | (*str - '0');
        str++;
    }
    return val;
}

static void parse_tar(uint8_t *ptr) {
    while (1) {
        struct tar_header *h = (struct tar_header *)ptr;
        if (h->name[0] == '\0') break; // End of archive

        uint64_t size = parse_octal(h->size);
        printf("Found file: %s (size: %d)\n", h->name, size);

        // ptr + 512 is where the ELF file data starts!
        
        // Jump to next header: header (512) + file data (aligned to 512)
        ptr += 512 + ((size + 511) & ~511);
    }
}

void init_rootfs(void) {
    // 1. Get the module from Limine
    if (!mod_req.response || mod_req.response->module_count == 0) {
        panic("No rootfs found.");
    }
    struct limine_file *file = mod_req.response->modules[0];
    
    // 2. Peek at the Gzip footer to see the UNCOMPRESSED size
    // Gzip stores the original size in the last 4 bytes of the file
    uint8_t *footer_ptr = (uint8_t *)(file->address + file->size - 4);
    uint32_t real_size = *(uint32_t *)footer_ptr;

    // 3. DYNAMIC ALLOCATION: Use malloc instead of manual memory map searching
    // This handles any size and ensures the heap won't overwrite our files
    void *safe_buffer = malloc(real_size);

    if (safe_buffer == NULL) {
        panic("Not enough memory to extract rootfs.");
    }

    // 4. Unzip into our new dynamic buffer
    ungzip(file->address, safe_buffer);

    // 5. Save the pointer globally for read_rootfs
    tar_archive_start = (uint8_t*)safe_buffer;
}

rootfs_file_t read_rootfs(const char *path) {
    rootfs_file_t result = { .data = NULL, .size = 0 };

    if (tar_archive_start == NULL) return result;

    uint8_t *ptr = tar_archive_start;

    while (1) {
        struct tar_header *h = (struct tar_header *)ptr;

        if (h->name[0] == '\0') break; 

        uint64_t size = parse_octal(h->size);

        // Check if this is the file we want
        if (strcmp(h->name, path) == 0) {
            result.data = (void *)(ptr + 512); // Data starts after header
            result.size = size;                // Store the parsed size
            return result;
        }

        // Jump to next header: 512 (header) + aligned data size
        ptr += 512 + ((size + 511) & ~511);
    }

    return result; // Not found
}