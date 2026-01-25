CC = gcc
CFLAGS = -I../include/ -ffreestanding -fno-stack-protector -fno-pic -fno-pie -no-pie -fno-lto -fno-stack-check -mno-red-zone -mcmodel=kernel -mno-red-zone -mcmodel=kernel -mno-80387 -mno-sse -mno-sse2 -mno-mmx -mabi=sysv
AS = $(CC)
AFLAGS = $(CFLAGS) -D__ASSEMBLY__

SRC = syscall.c syscall_entry.S syscall_handler.c
OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.S=.o)

all: $(OBJ)

%.o: %.c
	@printf "  %-7s %s\n" "CC" "$@"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@printf "  %-7s %s\n" "AS" "$@"
	@$(AS) $(AFLAGS) -c $< -o $@

