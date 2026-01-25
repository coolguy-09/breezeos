.global simple_trap
simple_trap:
    cli              # Disable interrupts so nothing else happens
    hlt              # Halt the CPU
    jmp simple_trap  # If a non-maskable interrupt wakes it, jump back to halt
