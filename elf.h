#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

// ELF Relocation Record Types
typedef enum {
    ELF_RELOC_STRING, // Relocation against .rodata
    ELF_RELOC_GLOBAL, // Relocation against .bss
    ELF_RELOC_EXTERN, // Relocation against undefined symbol (e.g. malloc)
    ELF_RELOC_FUNCTION // Internal function, already patched by Backend_Finalize, usually not needed in .rela.text
} ElfRelocType;

typedef struct {
    size_t patch_at;     // Offset in .text where the relocation applies
    ElfRelocType type;
    const char* symbol_name; // Only for ELF_RELOC_EXTERN
    size_t addend;       // Offset into .rodata or .bss
} ElfRelocation;

// Register an AOT relocation during Backend_Compile
void ELF_AddRelocation(size_t patch_at, ElfRelocType type, const char* symbol_name, size_t addend);

// Register a string literal to be placed in .rodata
// Returns the accumulated offset into the .rodata section
size_t ELF_AddString(const char* str, size_t length);

typedef struct {
    uint8_t* code;
    size_t size;
    size_t capacity;
} JITBuffer;

extern JITBuffer s_jit_buf;
extern uint64_t s_repl_globals[1024];

extern ElfRelocation* s_elf_relocs;
extern size_t s_elf_reloc_count;

extern char* s_elf_rodata;
extern size_t s_elf_rodata_size;

#endif
