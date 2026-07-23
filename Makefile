CC=gcc
# STAGE1 skips the self-hosted LLVM backend (backend_llvm.t, compiled by a
# bootstrapped torrent compiler, not part of this C source tree) -- without
# it, main.c's -llvm path needs torrent_backend_llvm() and fails to link.
CFLAGS=-O2 -w -DSTAGE1
CFLAGS_DEBUG=-Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -g -O0 -DSTAGE1
SRC=backend_x64.c codegen.c constexpr.c lexer.c main.c parser.c structs.c symtable.c types.c extern.c module.c elf.c reflections.c error.c match.c
torrent: $(SRC)
	$(CC) $(CFLAGS) -o torrent $(SRC) -ldl -Wl,--no-as-needed -Wl,-E
debug: $(SRC)
	$(CC) $(CFLAGS_DEBUG) -o torrent_debug $(SRC) -ldl -Wl,--no-as-needed -Wl,-E
clean:
	rm -f torrent torrent_debug
