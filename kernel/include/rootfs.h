#pragma once

#include <rootfs.h>

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];     // Size is in Octal ASCII!
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];     // "ustar"
    char version[2];
};

typedef struct {
    void* data;      // Pointer to the actual file content
    uint64_t size;   // Actual size of the file in bytes
} rootfs_file_t;

void init_rootfs(void);
rootfs_file_t read_rootfs(const char *path);