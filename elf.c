#include "compiler.h"
#include "elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELF_MAGIC "\x7f" "ELF"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

#define SHN_UNDEF     0
#define SHT_NULL      0
#define SHT_PROGBITS  1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA      4
#define SHT_NOBITS    8

#define SHF_WRITE     (1 << 0)
#define SHF_ALLOC     (1 << 1)
#define SHF_EXECINSTR (1 << 2)
#define SHF_INFO_LINK (1 << 6)

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC   2

#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))
#define ELF64_R_INFO(sym, type) (((uint64_t)(sym) << 32) + (uint64_t)(type))

#define R_X86_64_PC32   2
#define R_X86_64_PLT32  4

// Section indices
#define SHIDX_NULL     0
#define SHIDX_TEXT     1
#define SHIDX_RODATA   2
#define SHIDX_BSS      3
#define SHIDX_SYMTAB   4
#define SHIDX_STRTAB   5
#define SHIDX_SHSTRTAB 6
#define SHIDX_RELA_TEXT 7

#define NUM_SECTIONS   8

// Internal String Table Builder
typedef struct {
    char* data;
    size_t size;
    size_t cap;
} StrTable;

static void strtab_init(StrTable* st) {
    st->cap = 1024;
    st->data = malloc(st->cap);
    st->data[0] = '\0';
    st->size = 1;
}

static uint32_t strtab_add(StrTable* st, const char* str) {
    if (!str) return 0;
    size_t len = strlen(str);
    if (st->size + len + 1 > st->cap) {
        st->cap = (st->size + len + 1) * 2;
        st->data = realloc(st->data, st->cap);
    }
    uint32_t offset = st->size;
    memcpy(st->data + offset, str, len + 1);
    st->size += len + 1;
    return offset;
}

static void strtab_free(StrTable* st) {
    free(st->data);
}

// Size (in bytes) of the globals image, matching s_repl_globals[1024] (uint64_t)
// used by the JIT/REPL path. AOT's .data section mirrors that same capacity so
// global offsets computed by the frontend are valid in both backends.
#define GLOBALS_IMAGE_SIZE (1024 * 8)

void Backend_EmitELF(const char* output_filename) {
    // Collect all unique external symbols (for .symtab) and generate .rela.text
    size_t max_externs = s_elf_reloc_count;
    const char** extern_names = calloc(max_externs, sizeof(const char*));
    size_t extern_count = 0;
    
    for (size_t i = 0; i < s_elf_reloc_count; i++) {
        if (s_elf_relocs[i].type == ELF_RELOC_EXTERN) {
            bool found = false;
            for (size_t j = 0; j < extern_count; j++) {
                if (strcmp(extern_names[j], s_elf_relocs[i].symbol_name) == 0) {
                    found = true; break;
                }
            }
            if (!found) {
                extern_names[extern_count++] = s_elf_relocs[i].symbol_name;
            }
        }
    }
    
    // Prepare .strtab and .shstrtab
    StrTable strtab; strtab_init(&strtab);
    StrTable shstrtab; strtab_init(&shstrtab);
    
    uint32_t shname_text     = strtab_add(&shstrtab, ".text");
    uint32_t shname_rodata   = strtab_add(&shstrtab, ".rodata");
    // Globals now live in a real PROGBITS .data section, not .bss/NOBITS (see
    // data_image build below). SHIDX_BSS constant kept as-is to limit the diff;
    // only its shstrtab name, sh_type, and file content change.
    uint32_t shname_bss      = strtab_add(&shstrtab, ".data");
    uint32_t shname_symtab   = strtab_add(&shstrtab, ".symtab");
    uint32_t shname_strtab   = strtab_add(&shstrtab, ".strtab");
    uint32_t shname_shstrtab = strtab_add(&shstrtab, ".shstrtab");
    uint32_t shname_rela     = strtab_add(&shstrtab, ".rela.text");
    
    // Prepare .symtab
    SymbolTable* global_st = Get_SymTable();
    size_t sym_count = 1 + extern_count + 3 + global_st->count; // NULL + externs + sections(.text,.rodata,.bss) + globals
    Elf64_Sym* syms = calloc(sym_count, sizeof(Elf64_Sym));
    
    size_t sym_idx = 1;
    
    // Add external undefined symbols first (or globally? globals are typically at the end, but STB_GLOBAL must follow STB_LOCAL)
    // Actually, ELF requires all STB_LOCAL symbols to precede STB_GLOBAL symbols.
    // Let's do:
    // 0: NULL
    // 1: .text (LOCAL, SECTION)
    // 2: .rodata (LOCAL, SECTION)
    // 3: .bss (LOCAL, SECTION)
    // STB_GLOBAL starts here.
    
    syms[1].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE); syms[1].st_shndx = SHIDX_TEXT;
    syms[2].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE); syms[2].st_shndx = SHIDX_RODATA;
    syms[3].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE); syms[3].st_shndx = SHIDX_BSS;
    sym_idx = 4;
    
    size_t first_global_idx = sym_idx;
    
    // Build the initialized .data image for all globals. Globals must live in a
    // real PROGBITS section (not .bss/NOBITS) because .bss can only hold
    // zero-filled storage — any global with a nonzero constexpr initializer
    // would silently start at 0 at runtime if placed there. This mirrors the
    // JIT path's static-init step in main.c, which writes global_init /
    // global_bytes into s_repl_globals before main runs; AOT must bake the same
    // bytes into the file up front since there's no pre-main init phase.
    uint8_t* data_image = calloc(1, GLOBALS_IMAGE_SIZE);
    for (size_t i = 0; i < global_st->count; i++) {
        Symbol* s = global_st->symbols[i];
        if (s->is_extern || s->kind != SYM_GLOBAL || !s->has_init) continue;
        if (s->global_bytes) {
            uint64_t sz = Type_SizeOf(s->type);
            if (s->offset >= 0 && (size_t)s->offset + sz <= GLOBALS_IMAGE_SIZE) {
                memcpy(data_image + s->offset, s->global_bytes, sz);
            }
        } else {
            if (s->offset >= 0 && (size_t)s->offset + sizeof(int64_t) <= GLOBALS_IMAGE_SIZE) {
                memcpy(data_image + s->offset, &s->global_init, sizeof(int64_t));
            }
        }
    }
    // Same fix as the JIT path (main.c): a `const` aggregate declared inside a
    // function body has SYM_GLOBAL storage but lives in that function's scope
    // table, so the walk above cannot see it and its bytes would never be baked
    // into .data -- reading back as zeros. Emission keys on "has folded bytes".
    for (size_t i = 0; i < Global_EmitCount(); i++) {
        Symbol* s = Global_EmitAt(i);
        if (!s || s->is_extern || s->kind != SYM_GLOBAL || !s->has_init) continue;
        if (!s->global_bytes) continue;
        uint64_t sz = Type_SizeOf(s->type);
        if (s->offset >= 0 && (size_t)s->offset + sz <= GLOBALS_IMAGE_SIZE) {
            memcpy(data_image + s->offset, s->global_bytes, sz);
        }
    }
    
    // Track each SYM_GLOBAL's own .symtab index, keyed by its .data offset, so
    // relocations below can reference the specific global symbol instead of a
    // shared section symbol (which previously made every global resolve to the
    // same address).
    size_t* global_sym_by_offset_key = calloc(global_st->count, sizeof(size_t));
    int* global_sym_by_offset_val = calloc(global_st->count, sizeof(int));
    size_t global_offset_map_count = 0;
    
    // Globals from Torrent
    for (size_t i = 0; i < global_st->count; i++) {
        Symbol* s = global_st->symbols[i];
        if (s->is_extern) continue; // Externs are handled separately
        if (s->kind == SYM_FUNCTION || s->kind == SYM_GLOBAL) {
            const char* sym_export_name = (strcmp(s->name, "main") == 0) ? "torrent_main" : s->name;
            syms[sym_idx].st_name = strtab_add(&strtab, sym_export_name);
            syms[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, (s->kind == SYM_FUNCTION) ? STT_FUNC : STT_OBJECT);
            syms[sym_idx].st_shndx = (s->kind == SYM_FUNCTION) ? SHIDX_TEXT : SHIDX_BSS; // SHIDX_BSS slot now holds .data (renamed below)
            syms[sym_idx].st_value = s->offset; // Both function offset and .data offset map nicely
            syms[sym_idx].st_size = 0;
            if (s->kind == SYM_GLOBAL) {
                global_sym_by_offset_key[global_offset_map_count] = (size_t)s->offset;
                global_sym_by_offset_val[global_offset_map_count] = (int)sym_idx;
                global_offset_map_count++;
            }
            sym_idx++;
        }
    }
    
    // Undefined external symbols
    size_t extern_sym_base = sym_idx;
    for (size_t i = 0; i < extern_count; i++) {
        syms[sym_idx].st_name = strtab_add(&strtab, extern_names[i]);
        syms[sym_idx].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
        syms[sym_idx].st_shndx = SHN_UNDEF;
        sym_idx++;
    }
    
    // Prepare .rela.text
    Elf64_Rela* relas = calloc(s_elf_reloc_count, sizeof(Elf64_Rela));
    size_t rela_count = 0;
    
    for (size_t i = 0; i < s_elf_reloc_count; i++) {
        ElfRelocation* r = &s_elf_relocs[i];
        relas[rela_count].r_offset = r->patch_at;
        relas[rela_count].r_addend = r->addend - 4; // PC32 is relative to (patch_at + 4)
        
        if (r->type == ELF_RELOC_STRING) {
            relas[rela_count].r_info = ELF64_R_INFO(2, R_X86_64_PC32); // Symbol 2 is .rodata section symbol
        } else if (r->type == ELF_RELOC_GLOBAL) {
            // Relocate against the specific global's OWN symbol (found by
            // matching r->addend, the global's .data offset, against the offset
            // map built above), not the shared section symbol. Previously every
            // global relocation pointed at symbol 3 (the section symbol) with
            // r->addend as a generic addend, so every global's lea resolved to
            // the same address; and .bss being NOBITS meant initializers were
            // never written to the file regardless. Now each global gets its own
            // symbol whose st_value is its true offset, so the addend relative
            // to that symbol's own value is simply -4.
            int global_sym = 3; // fallback; should always be found
            for (size_t g = 0; g < global_offset_map_count; g++) {
                if ((int64_t)global_sym_by_offset_key[g] == r->addend) {
                    global_sym = global_sym_by_offset_val[g];
                    break;
                }
            }
            relas[rela_count].r_info = ELF64_R_INFO(global_sym, R_X86_64_PC32);
            relas[rela_count].r_addend = -4;
        } else if (r->type == ELF_RELOC_EXTERN) {
            // Find extern symbol index
            size_t ext_idx = 0;
            for (size_t j = 0; j < extern_count; j++) {
                if (strcmp(extern_names[j], r->symbol_name) == 0) {
                    ext_idx = extern_sym_base + j;
                    break;
                }
            }
            relas[rela_count].r_info = ELF64_R_INFO(ext_idx, R_X86_64_PLT32); // Use PLT32 for function calls
        } else {
            continue; // Skip function
        }
        rela_count++;
    }
    
    // Calculate offsets
    size_t current_offset = sizeof(Elf64_Ehdr) + NUM_SECTIONS * sizeof(Elf64_Shdr);
    
    Elf64_Shdr shdrs[NUM_SECTIONS] = {0};
    
    // .text
    shdrs[SHIDX_TEXT].sh_name = shname_text;
    shdrs[SHIDX_TEXT].sh_type = SHT_PROGBITS;
    shdrs[SHIDX_TEXT].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdrs[SHIDX_TEXT].sh_offset = current_offset;
    shdrs[SHIDX_TEXT].sh_size = s_jit_buf.size;
    shdrs[SHIDX_TEXT].sh_addralign = 16;
    current_offset += s_jit_buf.size;
    
    // .rodata
    shdrs[SHIDX_RODATA].sh_name = shname_rodata;
    shdrs[SHIDX_RODATA].sh_type = SHT_PROGBITS;
    shdrs[SHIDX_RODATA].sh_flags = SHF_ALLOC;
    shdrs[SHIDX_RODATA].sh_offset = current_offset;
    shdrs[SHIDX_RODATA].sh_size = s_elf_rodata_size;
    shdrs[SHIDX_RODATA].sh_addralign = 8;
    current_offset += s_elf_rodata_size;
    
    // .data (formerly .bss/NOBITS). NOBITS can only ever read back as zero,
    // which is exactly why AOT globals previously always read 0 regardless of
    // their initializer. PROGBITS gives it real file bytes.
    shdrs[SHIDX_BSS].sh_name = shname_bss;
    shdrs[SHIDX_BSS].sh_type = SHT_PROGBITS;
    shdrs[SHIDX_BSS].sh_flags = SHF_ALLOC | SHF_WRITE;
    shdrs[SHIDX_BSS].sh_offset = current_offset;
    shdrs[SHIDX_BSS].sh_size = GLOBALS_IMAGE_SIZE; // matches s_repl_globals size
    shdrs[SHIDX_BSS].sh_addralign = 8;
    current_offset += GLOBALS_IMAGE_SIZE; // now occupies real file space
    
    // .symtab
    shdrs[SHIDX_SYMTAB].sh_name = shname_symtab;
    shdrs[SHIDX_SYMTAB].sh_type = SHT_SYMTAB;
    shdrs[SHIDX_SYMTAB].sh_offset = current_offset;
    shdrs[SHIDX_SYMTAB].sh_size = sym_idx * sizeof(Elf64_Sym);
    shdrs[SHIDX_SYMTAB].sh_entsize = sizeof(Elf64_Sym);
    shdrs[SHIDX_SYMTAB].sh_link = SHIDX_STRTAB;
    shdrs[SHIDX_SYMTAB].sh_info = first_global_idx;
    shdrs[SHIDX_SYMTAB].sh_addralign = 8;
    current_offset += shdrs[SHIDX_SYMTAB].sh_size;
    
    // .strtab
    shdrs[SHIDX_STRTAB].sh_name = shname_strtab;
    shdrs[SHIDX_STRTAB].sh_type = SHT_STRTAB;
    shdrs[SHIDX_STRTAB].sh_offset = current_offset;
    shdrs[SHIDX_STRTAB].sh_size = strtab.size;
    shdrs[SHIDX_STRTAB].sh_addralign = 1;
    current_offset += strtab.size;
    
    // .shstrtab
    shdrs[SHIDX_SHSTRTAB].sh_name = shname_shstrtab;
    shdrs[SHIDX_SHSTRTAB].sh_type = SHT_STRTAB;
    shdrs[SHIDX_SHSTRTAB].sh_offset = current_offset;
    shdrs[SHIDX_SHSTRTAB].sh_size = shstrtab.size;
    shdrs[SHIDX_SHSTRTAB].sh_addralign = 1;
    current_offset += shstrtab.size;
    
    // .rela.text
    shdrs[SHIDX_RELA_TEXT].sh_name = shname_rela;
    shdrs[SHIDX_RELA_TEXT].sh_type = SHT_RELA;
    shdrs[SHIDX_RELA_TEXT].sh_flags = SHF_INFO_LINK;
    shdrs[SHIDX_RELA_TEXT].sh_offset = current_offset;
    shdrs[SHIDX_RELA_TEXT].sh_size = rela_count * sizeof(Elf64_Rela);
    shdrs[SHIDX_RELA_TEXT].sh_entsize = sizeof(Elf64_Rela);
    shdrs[SHIDX_RELA_TEXT].sh_link = SHIDX_SYMTAB;
    shdrs[SHIDX_RELA_TEXT].sh_info = SHIDX_TEXT;
    shdrs[SHIDX_RELA_TEXT].sh_addralign = 8;
    current_offset += shdrs[SHIDX_RELA_TEXT].sh_size;
    
    // Prepare Header
    Elf64_Ehdr ehdr = {0};
    memcpy(ehdr.e_ident, ELF_MAGIC, 4);
    ehdr.e_ident[4] = 2; // 64-bit
    ehdr.e_ident[5] = 1; // Little endian
    ehdr.e_ident[6] = 1; // Version 1
    ehdr.e_type = 1;     // ET_REL (Relocatable)
    ehdr.e_machine = 62; // AMD x86-64
    ehdr.e_version = 1;
    ehdr.e_shoff = sizeof(Elf64_Ehdr); // Section headers immediately follow ELF header
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = NUM_SECTIONS;
    ehdr.e_shstrndx = SHIDX_SHSTRTAB;
    
    // Write out
    FILE* f = fopen(output_filename, "wb");
    if (!f) {
        perror("fopen");
        return;
    }
    
    fwrite(&ehdr, 1, sizeof(Elf64_Ehdr), f);
    fwrite(shdrs, 1, sizeof(shdrs), f);
    
    fwrite(s_jit_buf.code, 1, s_jit_buf.size, f);
    fwrite(s_elf_rodata, 1, s_elf_rodata_size, f);
    fwrite(data_image, 1, GLOBALS_IMAGE_SIZE, f); // real initializer bytes, not skipped like .bss was
    fwrite(syms, 1, shdrs[SHIDX_SYMTAB].sh_size, f);
    fwrite(strtab.data, 1, strtab.size, f);
    fwrite(shstrtab.data, 1, shstrtab.size, f);
    fwrite(relas, 1, shdrs[SHIDX_RELA_TEXT].sh_size, f);
    
    fclose(f);
    
    printf("Emitted relocatable ELF: %s\n", output_filename);
    
    free(extern_names);
    free(syms);
    free(relas);
    free(data_image);
    free(global_sym_by_offset_key);
    free(global_sym_by_offset_val);
    strtab_free(&strtab);
    strtab_free(&shstrtab);
}
