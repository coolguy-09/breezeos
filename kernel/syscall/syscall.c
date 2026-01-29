#include <stdint.h>

#define STAR_MSR  0xC0000081
#define LSTAR_MSR 0xC0000082
#define SFMASK_MSR 0xC0000084

extern void syscall_entry(); // We'll write this in assembly

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
        : "memory"
    );
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
        : "memory"
    );
    return ((uint64_t)high << 32) | low;
}

void init_syscall(void) {
    // 1. Entry Point: Where the CPU jumps when 'syscall' is executed
    wrmsr(LSTAR_MSR, (uint64_t)syscall_entry);

    // 2. STAR MSR: Defines the segments for both entry and exit.
    // [48:63] User Segment Base: Points to User Data (0x18). 
    //         sysret will use this + 8 for CS and + 0 for SS.
    // [32:47] Kernel Segment Base: Points to Kernel Code (0x08).
    //         syscall will use this + 0 for CS and + 8 for SS.
    
    // We use (0x18 | 3) for the user base to set the Request Privilege Level to Ring 3.
    uint64_t star = ((uint64_t)(0x18 | 3) << 48) | ((uint64_t)0x08 << 32);
    wrmsr(STAR_MSR, star);

    // 3. SFMASK: System Call Flag Mask
    // This tells the CPU which bits in RFLAGS to CLEAR when entering the kernel.
    // 0x200 = Interrupt Flag. This ensures interrupts are DISABLED 
    // automatically when the syscall starts so we don't crash.
    wrmsr(SFMASK_MSR, 0x200);

    // 4. EFER: Extended Feature Enable Register
    // Bit 0 is 'SCE' (System Call Enable). Without this, 'syscall' is an invalid instruction.
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1);
}