#pragma once

#include <stdint.h>
#include <rootfs.h>

#define ELF_MAGIC 0x464C457F // "\x7FELF"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;      // The "start" address!
    uint64_t e_phoff;      // Program header table offset
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;      // Number of program headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;      // Where the data wants to live in RAM
    uint64_t p_paddr;
    uint64_t p_filesz;     // Size in the file
    uint64_t p_memsz;      // Size in RAM (can be bigger than filesz for BSS)
    uint64_t p_align;
} Elf64_Phdr;

#define PT_LOAD 1

typedef struct {
    uint64_t rsp; // The saved stack pointer
    // You can add process IDs, page table pointers, etc. here later
} task_t;

task_t* create_task(uint64_t entry_point);
uint64_t get_address_elf(rootfs_file_t file);