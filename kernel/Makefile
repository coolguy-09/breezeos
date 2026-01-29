LD = ld
LDFLAGS = -T linker.ld
OUTFILE = kernel.elf

SRC = main/entry.S main/limine_req.c main/kernel.c main/string.c io/framebuffer.c io/terminal.c main/panic.c main/rootfs.c main/gzip.c mm/mm.c main/halt.c io/io.c syscall/syscall.c syscall/syscall_entry.S syscall/syscall_handler.c main/idt.c
OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.S=.o)
FOLDERS = main/*.o io/*.o mm/*.o syscall/*.o

all: $(OUTFILE)

$(OBJ): $(SRC)
	@make -C main
	@make -C io
	@make -C mm
	@make -C syscall

$(OUTFILE): $(OBJ)
	@printf "  %-7s %s\n" "LD" "$@"
	@$(LD) $(LDFLAGS) -o $@ $^

clean:
	@printf "  %-7s %s\n" "CLEAN" "$(FOLDERS) $(OUTFILE)"
	@rm -f $(OBJ) $(OUTFILE)
