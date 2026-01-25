#include <stdint.h>
#include <string.h>
#include <terminal.h>
#include <rootfs.h>
#include <panic.h>
#include <elf.h>
#include <mm.h>

extern uint64_t hhdm_offset;

static inline uint64_t read_cr3(void) {
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

// Full TLB flush to ensure the CPU sees new page table mappings immediately
static inline void flush_tlb() {
    asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

void map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(read_cr3() + hhdm_offset);

    uint64_t* tables[3] = {pml4, NULL, NULL};
    uint64_t indices[3] = {pml4_idx, pdpt_idx, pd_idx};

    for (int i = 0; i < 3; i++) {
        if (!(tables[i][indices[i]] & 1)) {
            void* new_table = malloc(8192);
            uint64_t aligned = ((uint64_t)new_table + 4095) & ~4095;
            memset((void*)aligned, 0, 4096);
            tables[i][indices[i]] = (aligned - hhdm_offset) | 0x7;
        }
        if (i < 2) {
            tables[i+1] = (uint64_t*)((tables[i][indices[i]] & ~0xFFF) + hhdm_offset);
        }
    }

    uint64_t* pt = (uint64_t*)((tables[2][pd_idx] & ~0xFFF) + hhdm_offset);
    pt[pt_idx] = phys | flags;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

task_t* create_task(uint64_t entry_point) {
    uint64_t stack_phys_raw = (uint64_t)malloc(16384 + 4096);
    uint64_t stack_phys = (stack_phys_raw + 4095) & ~4095;
    uint64_t stack_virt = 0x70000000000; 

    for (int i = 0; i < 4; i++) {
        map_page(stack_virt + (i * 4096), (stack_phys - hhdm_offset) + (i * 4096), 0x7);
    }

    uint64_t* stack_top = (uint64_t*)(stack_phys + 16384);
    
    *(--stack_top) = 0x30; 
    *(--stack_top) = stack_virt + 16384; 
    *(--stack_top) = 0x202; 
    *(--stack_top) = 0x28; 
    *(--stack_top) = entry_point; 

    for(int i = 0; i < 15; i++) {
        *(--stack_top) = 0; 
    }

    task_t* t = malloc(sizeof(task_t));
    t->rsp = (uint64_t)stack_top; 
    return t;
}

uint64_t get_address_elf(rootfs_file_t file) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)file.data;

    if (*(uint32_t *)header->e_ident != ELF_MAGIC) {
        panic("Not a valid ELF file.");
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((uint8_t *)file.data + header->e_phoff);
    
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t virt_start = phdrs[i].p_vaddr & ~0xFFF;
            uint64_t virt_end = (phdrs[i].p_vaddr + phdrs[i].p_memsz + 4095) & ~0xFFF;

            for (uint64_t v = virt_start; v < virt_end; v += 4096) {
                void* phys = malloc(8192);
                uint64_t aligned_phys = ((uint64_t)phys + 4095) & ~4095;
                // Use 0x7 (User bit) even for kernel tasks if they live in lower-half memory
                map_page(v, aligned_phys - hhdm_offset, 0x7);
            }

            // Copy data into the newly mapped virtual memory
            memcpy((void *)phdrs[i].p_vaddr, (uint8_t *)file.data + phdrs[i].p_offset, phdrs[i].p_filesz);

            // Zero out remaining memory (BSS section)
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset((uint8_t*)phdrs[i].p_vaddr + phdrs[i].p_filesz, 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
            }
        }
    }

    // Critical: Flush TLB so the CPU knows about the new ELF addresses
    flush_tlb();

    printf("ELF parsed. Entry point is: %p\n", (void*)header->e_entry);
    return header->e_entry; 
}