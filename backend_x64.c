#define _GNU_SOURCE
#include "compiler.h"
#include "codegen.h"
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct LoopContext {
    size_t start_offset;       // top of loop (condition re-test)
    size_t* continue_offsets;  // `continue` jump sites, backpatched to the continue target
    size_t continue_count;
    size_t continue_capacity;
    size_t* break_offsets;
    size_t break_count;
    size_t break_capacity;
    struct LoopContext* parent;
    struct DeferCtx* defer_base;
} LoopContext;

typedef struct FuncContext {
    size_t* ret_offsets;   // jmp rel32 slots to backpatch to the epilogue
    size_t ret_count;
    size_t ret_capacity;
    Type* ret_type;        // function's declared return type (NULL = returns nothing)
    bool aggregate_ret;    // returns a struct/array by value (sret hidden-pointer ABI)
    int sret_slot;         // rbp-relative offset where the hidden result pointer is spilled
    int current_local_offset; // tracks the current RBP offset for allocating locals/params
} FuncContext;

static FuncContext* s_func = NULL; // current function being compiled

typedef struct DeferCtx {
    ASTNode* stmt;
    struct DeferCtx* next;
} DeferCtx;

static DeferCtx* s_defer_stack = NULL;

#include "elf.h"

bool g_aot_mode = false;

static void func_add_ret(FuncContext* fc, size_t off) {
    if (fc->ret_count >= fc->ret_capacity) {
        fc->ret_capacity = fc->ret_capacity ? fc->ret_capacity * 2 : 4;
        fc->ret_offsets = (size_t*)realloc(fc->ret_offsets, fc->ret_capacity * sizeof(size_t));
    }
    fc->ret_offsets[fc->ret_count++] = off;
}

// Expose the global JIT buffer and static globals to elf.c
JITBuffer s_jit_buf = {0};
uint64_t s_repl_globals[1024];

ElfRelocation* s_elf_relocs = NULL;
size_t s_elf_reloc_count = 0;
static size_t s_elf_reloc_cap = 0;

char* s_elf_rodata = NULL;
size_t s_elf_rodata_size = 0;
static size_t s_elf_rodata_cap = 0;

void ELF_AddRelocation(size_t patch_at, ElfRelocType type, const char* symbol_name, size_t addend) {
    if (s_elf_reloc_count >= s_elf_reloc_cap) {
        s_elf_reloc_cap = s_elf_reloc_cap ? s_elf_reloc_cap * 2 : 64;
        s_elf_relocs = realloc(s_elf_relocs, s_elf_reloc_cap * sizeof(ElfRelocation));
    }
    s_elf_relocs[s_elf_reloc_count].patch_at = patch_at;
    s_elf_relocs[s_elf_reloc_count].type = type;
    s_elf_relocs[s_elf_reloc_count].symbol_name = symbol_name;
    s_elf_relocs[s_elf_reloc_count].addend = addend;
    s_elf_reloc_count++;
}

size_t ELF_AddString(const char* str, size_t length) {
    size_t offset = s_elf_rodata_size;
    size_t new_size = offset + length + 1;
    if (new_size > s_elf_rodata_cap) {
        s_elf_rodata_cap = new_size > s_elf_rodata_cap * 2 ? new_size : s_elf_rodata_cap * 2;
        if (s_elf_rodata_cap < 1024) s_elf_rodata_cap = 1024;
        s_elf_rodata = realloc(s_elf_rodata, s_elf_rodata_cap);
    }
    memcpy(s_elf_rodata + offset, str, length);
    s_elf_rodata[offset + length] = '\0';
    s_elf_rodata_size = new_size;
    return offset;
}

// Call fixups: every `call rel32` records its patch site + target function symbol,
// resolved at Backend_Finalize once all functions have offsets. This makes call
// order irrelevant (a function may call one defined later — no forward decls).
typedef struct { size_t patch_at; Symbol* target; } CallFixup;
static CallFixup* s_fixups = NULL;
static size_t s_fixup_count = 0, s_fixup_cap = 0;

static void add_fixup(size_t patch_at, Symbol* target) {
    if (s_fixup_count >= s_fixup_cap) {
        s_fixup_cap = s_fixup_cap ? s_fixup_cap * 2 : 64;
        s_fixups = realloc(s_fixups, s_fixup_cap * sizeof(CallFixup));
    }
    s_fixups[s_fixup_count].patch_at = patch_at;
    s_fixups[s_fixup_count].target = target;
    s_fixup_count++;
}

// ===========================================================================
// Generics (stage 1: functions only). A generic function is never compiled at
// its definition; each [T,...] instantiation is cloned, type-substituted, and
// compiled on demand. Instantiations are cached by (generic, concrete type args)
// so repeated uses share one specialization. Discovery is on-demand and
// transitive (a generic body may instantiate further generics), realized via a
// pending queue drained after the main compile pass and before finalize.
// ===========================================================================

void Backend_Init(void) {
    size_t page_size = 4096 * 256; // 1MB initial
    uint8_t* mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    s_jit_buf.code = mem;
    s_jit_buf.capacity = page_size;
    s_jit_buf.size = 0;
}

static void emit_byte(JITBuffer* buf, uint8_t b) {
    if (buf->size >= buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        uint8_t* new_mem = mremap(buf->code, buf->capacity, new_cap, MREMAP_MAYMOVE);
        if (new_mem == MAP_FAILED) {
            fprintf(stderr, "JIT Buffer Overflow (mremap failed)\n");
            exit(1);
        }
        buf->code = new_mem;
        buf->capacity = new_cap;
    }
    buf->code[buf->size++] = b;
}

static void emit_bytes(JITBuffer* buf, const uint8_t* b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        emit_byte(buf, b[i]);
    }
}

static void emit_u64(JITBuffer* buf, uint64_t val) {
    emit_bytes(buf, (const uint8_t*)&val, 8);
}

static void emit_u32(JITBuffer* buf, uint32_t val) {
    emit_bytes(buf, (const uint8_t*)&val, 4);
}

// Emits code that leaves a real, callable address for `target_sym` in rax:
// an extern function's dlsym-resolved address, or a RIP-relative lea into
// the JIT buffer (patched by add_fixup once every function has a final
// offset). This is the ONLY correct way to turn a function Symbol* into a
// runtime value — the address is not known until codegen/finalize, so it
// can never be baked in as a raw pointer literal. Used both for an ordinary
// function name in value position AND for a const-generic fn-param literal
// (LIT_FN_SYMBOL) wherever that literal is compiled as a value rather than
// intercepted by the direct-call shortcut in AST_CALL — one place that
// knows how to do this, instead of a growing whitelist of AST shapes that
// each have to remember to do it correctly.
static void emit_fn_symbol_value(JITBuffer* buf, Symbol* target_sym) {
    if (target_sym->is_extern) {
        // An extern function was never compiled into the JIT buffer — it
        // lives at a real address dlsym resolves, entirely outside
        // buf->code. add_fixup computes a RIP-relative offset INTO
        // buf->code, which is meaningless here.
        void* addr = Extern_Resolve(target_sym->name);
        // movabs rax, addr — same encoding AST_STRING's JIT-mode branch
        // already uses for embedding a real host address.
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, (uint64_t)addr);
        return;
    }
    emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x05); // lea rax, [rip + rel32]
    add_fixup(buf->size, target_sym);
    emit_u32(buf, 0);
}

static void compile_node_ctx(JITBuffer* buf, ASTNode* node, LoopContext* loop);

// Put the address of a variable's storage into rax.
static void emit_var_addr(JITBuffer* buf, Symbol* sym) {
    if (sym->kind == SYM_GLOBAL) {
        if (g_aot_mode) {
            emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x05); // lea rax, [rip + offset]
            ELF_AddRelocation(buf->size, ELF_RELOC_GLOBAL, NULL, sym->offset);
            emit_u32(buf, 0);
        } else {
            emit_byte(buf, 0x49); emit_byte(buf, 0xbf); emit_u64(buf, (uint64_t)&s_repl_globals[0]); // movabs r15, base
            emit_byte(buf, 0x49); emit_byte(buf, 0x8d); emit_byte(buf, 0x87); emit_u32(buf, sym->offset); // lea rax,[r15+off]
        }
    } else {
        emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x85);
        emit_u32(buf, (uint32_t)(-sym->offset)); // lea rax,[rbp-off]
    }
}

// Is this node an lvalue (has a stable storage address we can take)? Idents,
// derefs, and field/index chains rooted in one are. A call, if-expr, literal,
// etc. is an rvalue — for an aggregate rvalue, its value already IS the address
// of its (temp) storage, so field access uses compile_node_ctx instead.

// Compound-assign single-eval memo: `a op= b` desugars (parser.c) to
// `a = a <op> b` with the SAME `a` subtree pointed to from both the assign
// target and the binop's left operand. If `a` is a complex lvalue (a call in
// an index/field base, e.g. `arr[idx()]`), naively walking it twice would
// call idx() twice and, worse, compute two DIFFERENT addresses from the two
// results -- a load from one, a store to the other. Silent corruption.
//
// Fix: AST_ASSIGN codegen computes `a`'s address once, spills it to a frame
// slot, and pushes a (node, slot) entry here keyed by the shared node
// pointer. compile_lvalue checks this stack first and, on a hit, just loads
// the spilled address instead of recomputing it. A small fixed stack is
// enough: entries are only live for the duration of compiling one compound
// assign's binop, and nesting depth is bounded by expression nesting depth.
#define CA_MEMO_MAX 64
static ASTNode* s_ca_node[CA_MEMO_MAX];
static int s_ca_slot[CA_MEMO_MAX];
static int s_ca_depth = 0;

static bool ca_memo_lookup(ASTNode* node, int* out_slot) {
    for (int i = s_ca_depth - 1; i >= 0; i--) {
        if (s_ca_node[i] == node) { *out_slot = s_ca_slot[i]; return true; }
    }
    return false;
}

static void compile_lvalue(JITBuffer* buf, ASTNode* node, LoopContext* loop) {
    int memo_slot;
    if (ca_memo_lookup(node, &memo_slot)) {
        // Second walk of a shared compound-assign lvalue: load the address we
        // already computed and spilled, instead of recomputing it (which for
        // a complex lvalue would both double-eval a side effect and risk
        // landing on a different address the second time).
        emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x85); // mov rax, [rbp-slot]
        emit_u32(buf, (uint32_t)(-memo_slot));
        return;
    }
    if (node->type == AST_IDENT) {
        Symbol* sym = node->ident.sym;
        if (!sym) {
            Error_AtNode(node, "unresolved variable", NULL);
        }
        
        if (sym->kind == SYM_GLOBAL) {
            if (g_aot_mode) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x05); // lea rax, [rip + offset]
                ELF_AddRelocation(buf->size, ELF_RELOC_GLOBAL, NULL, sym->offset);
                emit_u32(buf, 0);
            } else {
                // movabs r15, &s_repl_globals
                emit_byte(buf, 0x49); emit_byte(buf, 0xbf); emit_u64(buf, (uint64_t)&s_repl_globals[0]);
                // lea rax, [r15 + offset] (49 8d 87 <offset32>)
                emit_byte(buf, 0x49); emit_byte(buf, 0x8d); emit_byte(buf, 0x87);
                emit_u32(buf, sym->offset);
            }
        } else if (sym->kind == SYM_LOCAL) {
            // lea rax, [rbp - offset]
            emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x85);
            uint32_t neg_offset = -sym->offset; // 2s complement
            emit_u32(buf, neg_offset);
        }
    } else if (node->type == AST_DEREF) {
        compile_node_ctx(buf, node->unary, loop);
    } else if (node->type == AST_FIELD) {
        // Address of base.field = base_address + field->offset.
        // base_address: if base is a struct value -> its lvalue address;
        //               if base is a pointer-to-struct -> the pointer value (auto-deref).
        Type_Infer(node); // ensure field.field/sdef are resolved (needed for cloned generic ASTs)
        Type* bt = Type_Infer(node->field.base);
        if (bt && bt->cls == TYPE_POINTER) {
            compile_node_ctx(buf, node->field.base, loop); // pointer value in rax
        } else if (base_is_lvalue(node->field.base)) {
            compile_lvalue(buf, node->field.base, loop);   // struct base address in rax
        } else {
            // RVALUE struct base (a call result, if-expr, etc.): aggregate rvalues
            // already evaluate to the *address* of their storage in this backend
            // (sret pointer / materialized temp slot), so use the value directly as
            // the base address. Enables `mk().field`, `arr[i](x).field`, etc.
            compile_node_ctx(buf, node->field.base, loop);
        }
        uint64_t off = node->field.field ? node->field.field->offset : 0;
        if (off) {
            // add rax, imm32
            emit_byte(buf, 0x48); emit_byte(buf, 0x05); emit_u32(buf, (uint32_t)off);
        }
    } else if (node->type == AST_INDEX) {
        // Address of a[i] = base_address + i * elem_size.
        Type* bt = Type_Infer(node->index.base);
        Type* elem;
        // Base address into rax: array value -> lvalue address; pointer -> its value.
        if (bt && bt->cls == TYPE_POINTER) {
            // "array indexing auto-derefs through a pointer" (specs.md §8): p[i]
            // means (*p)[i]. When the pointee is itself an array type (u32[8]*),
            // the element/stride we want is the pointee ARRAY's element type,
            // not the pointee type itself -- else p[i] strides by sizeof(u32[8])
            // (whole-array stride, C pointer-to-array semantics) instead of
            // sizeof(u32) (one auto-derefed level in, then index), and lands on
            // entirely the wrong address. Mirrors Type_Infer's AST_INDEX case,
            // which already made this same distinction for the *type* of p[i]
            // without the matching address-arithmetic fix landing here too.
            Type* pointee = bt->pointer_base;
            elem = (pointee && pointee->cls == TYPE_ARRAY) ? pointee->array.element : pointee;
            compile_node_ctx(buf, node->index.base, loop); // pointer value
        } else {
            elem = bt ? bt->array.element : NULL;
            compile_lvalue(buf, node->index.base, loop);   // array base address
        }
        uint64_t esz = elem ? Type_SizeOf(elem) : 8;
        emit_byte(buf, 0x50); // push base address
        compile_node_ctx(buf, node->index.index, loop);    // index in rax
        // rax = index * esz   (imul rax, rax, imm32)
        emit_byte(buf, 0x48); emit_byte(buf, 0x69); emit_byte(buf, 0xc0); emit_u32(buf, (uint32_t)esz);
        emit_byte(buf, 0x59); // pop rcx (base)
        emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xc8); // add rax, rcx -> element address
    } else {
        // Last resort: if this is an aggregate rvalue (e.g. a function call returning
        // a struct), the backend already materializes it to a stack temp via sret and
        // returns that temp's address in rax. So compile_node_ctx gives us the address
        // directly. Covers `f().method()` call-chaining: `&(f())` → address of temp.
        Type* t = Type_Infer(node);
        if (t && t->cls == TYPE_STRUCT) {
            compile_node_ctx(buf, node, loop);
            return;
        }
        Error_AtNode(node, "not an lvalue", NULL);
    }
}

// x86 AggBinopSink: emits code for one lane of an aggregate binop. rax=&L,
// rcx=&R hold operand addresses for the whole traversal (agg_binop_apply
// calls this once per lane; the addresses don't move between calls).
// EQ/NEQ mode: XOR each lane into rdx (the running diff accumulator set up
// by the caller before agg_binop_apply runs); arithmetic mode: compute into
// rsi and store at [rdx+offset], where rdx holds the fresh stack temp's
// address (also set up by the caller). Same op set, same byte sequences as
// the pre-migration inline version -- only now driven per-lane by the shared
// traversal instead of a hand-inlined loop over Agg_Lanes.
typedef struct {
    JITBuffer* buf;
    FuncContext* func;
    bool is_eq_mode;
    int dst_off; // arithmetic mode only: [rbp-dst_off] is the result temp
} X64AggBinopCtx;

static bool x64_agg_do_lane(void* ctxp, ASTNodeType op, uint64_t offset, int width, bool is_eq) {
    X64AggBinopCtx* c = (X64AggBinopCtx*)ctxp;
    JITBuffer* buf = c->buf;
    uint32_t o = (uint32_t)offset; int w = width;
    if (is_eq) {
        // Load L lane -> rsi, XOR R lane in place, OR into the rdx accumulator.
        if (w==8)      { emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xb0);emit_u32(buf,o);   // mov rsi,[rax+o]
                         emit_byte(buf,0x48);emit_byte(buf,0x33);emit_byte(buf,0xb1);emit_u32(buf,o); } // xor rsi,[rcx+o]
        else if (w==4) { emit_byte(buf,0x8b);emit_byte(buf,0xb0);emit_u32(buf,o);                        // mov esi,[rax+o]
                         emit_byte(buf,0x33);emit_byte(buf,0xb1);emit_u32(buf,o); }                      // xor esi,[rcx+o]
        else if (w==2) { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb7);emit_byte(buf,0xb0);emit_u32(buf,o); // movzx rsi,word[rax+o]
                         emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb7);emit_byte(buf,0xb9);emit_u32(buf,o); // movzx rdi,word[rcx+o]
                         emit_byte(buf,0x48);emit_byte(buf,0x31);emit_byte(buf,0xfe); }                  // xor rsi,rdi
        else           { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xb0);emit_u32(buf,o); // movzx rsi,byte[rax+o]
                         emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xb9);emit_u32(buf,o); // movzx rdi,byte[rcx+o]
                         emit_byte(buf,0x48);emit_byte(buf,0x31);emit_byte(buf,0xfe); }                  // xor rsi,rdi
        emit_byte(buf,0x48);emit_byte(buf,0x09);emit_byte(buf,0xf2);          // or rdx, rsi
        return true;
    }
    uint8_t opb = 0;
    switch (op) {
        case AST_ADD: opb=0x01; break; case AST_SUB: opb=0x29; break;
        case AST_BIT_AND: opb=0x21; break; case AST_BIT_OR: opb=0x09; break;
        case AST_BIT_XOR: opb=0x31; break; case AST_MUL: opb=0xaf; break; // imul special-cased
        default: return false; // agg_binop_apply already gated the op set; unreachable
    }
    // load L lane -> rsi (zero-extended), R lane -> rdi
    if (w==8)      { emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xb0);emit_u32(buf,o);
                     emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xb9);emit_u32(buf,o); }
    else if (w==4) { emit_byte(buf,0x8b);emit_byte(buf,0xb0);emit_u32(buf,o);
                     emit_byte(buf,0x8b);emit_byte(buf,0xb9);emit_u32(buf,o); }
    else if (w==2) { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb7);emit_byte(buf,0xb0);emit_u32(buf,o);
                     emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb7);emit_byte(buf,0xb9);emit_u32(buf,o); }
    else           { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xb0);emit_u32(buf,o);
                     emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xb9);emit_u32(buf,o); }
    if (op == AST_MUL) { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xaf);emit_byte(buf,0xf7); } // imul rsi,rdi
    else                { emit_byte(buf,0x48);emit_byte(buf,opb);emit_byte(buf,0xfe); }                      // op rsi,rdi
    // store rsi -> [rdx+o] at lane width (truncation = correct per-lane wrap)
    if (w==8)      { emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xb2);emit_u32(buf,o); }
    else if (w==4) { emit_byte(buf,0x89);emit_byte(buf,0xb2);emit_u32(buf,o); }
    else if (w==2) { emit_byte(buf,0x66);emit_byte(buf,0x89);emit_byte(buf,0xb2);emit_u32(buf,o); }
    else           { emit_byte(buf,0x40);emit_byte(buf,0x88);emit_byte(buf,0xb2);emit_u32(buf,o); }
    return true;
}

// EQ/NEQ finalization happens OUTSIDE agg_binop_apply in this backend (the
// sete/setne + mov-to-rax sequence needs to run after the traversal, using
// buf/node->type which the generic caller doesn't have) -- so this callback
// is a no-op; the caller does the finish step itself. Still required by the
// AggBinopSink interface so agg_binop_apply's contract is honored uniformly.
static bool x64_agg_finish_eq(void* ctxp, bool is_eq, int64_t* out_bool) {
    (void)ctxp; (void)is_eq; (void)out_bool;
    return true;
}

// Load a value of width w from [rax] into rax, zero/sign-extended per signedness.
static void emit_load_from_rax(JITBuffer* buf, int w, bool sgn) {
    switch (w) {
        case 1:
            if (sgn) { emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xbe); emit_byte(buf, 0x00); } // movsx rax,[rax]
            else     { emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb6); emit_byte(buf, 0x00); } // movzx rax,[rax]
            break;
        case 2:
            if (sgn) { emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xbf); emit_byte(buf, 0x00); } // movsx rax,[rax]
            else     { emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb7); emit_byte(buf, 0x00); } // movzx rax,[rax]
            break;
        case 4:
            if (sgn) { emit_byte(buf, 0x48); emit_byte(buf, 0x63); emit_byte(buf, 0x00); } // movsxd rax,[rax]
            else     { emit_byte(buf, 0x8b); emit_byte(buf, 0x00); }                       // mov eax,[rax] (zero-extends)
            break;
        default: // 8
            emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x00); // mov rax,[rax]
            break;
    }
}

// Copy `size` bytes: dst address in rdi, src address in rsi (rep movsb).
// Clobbers rdi, rsi, rcx. Callers must set rdi/rsi first.
static void emit_memcpy_rdi_rsi(JITBuffer* buf, uint64_t size) {
    // mov ecx, size  (size fits in 32 bits for our struct sizes)
    emit_byte(buf, 0xb9); emit_u32(buf, (uint32_t)size);
    // rep movsb
    emit_byte(buf, 0xf3); emit_byte(buf, 0xa4);
}

// --- heap allocation primitive (new / delete) ---------------------------------
// `new`/`delete` lower to a call to an allocator. The language defines only the
// interface (alloc(size)->ptr, free(ptr)); the environment supplies the impl. In
// the hosted JIT the JIT'd code shares the compiler's address space, so we target
// libc malloc/free directly. On TotemOS these addresses are swapped for Totem's
// allocator with no change to the language or this codegen. The byte size is ALWAYS
// computed from the type via Type_SizeOf by the caller — `new` never inspects the
// type's kind, so it composes with arbitrarily nested and (later) generic types.

// Call an absolute function address. The arguments must already be in the SysV
// argument registers (rdi, rsi, ...). Result comes back in rax. We force 16-byte
// stack alignment around the call (SSE-using callees like libc require it): save
// the original rsp in rbx-style scratch on the stack, align, call, restore.
// `vararg_float_count` is -1 for a non-variadic call, or the number of xmm
// registers actually used by THIS call for a variadic one — must be applied
// to AL here, AFTER loading fnaddr into rax (which clobbers any al value the
// caller set beforehand), not before. Missing this ordering was the actual
// bug: an upstream `mov al, n` followed by this function's `mov rax, fnaddr`
// silently overwrote al with the low byte of the function's address instead
// of the intended float count, so printf never saw the real xmm count.
// addr_in_rax: when true, the callee address is already sitting in rax at
// call time (an indirect call through a fn-ptr value) rather than known at
// compile time as fnaddr — used by the indirect-call site below. Either way
// this is the SAME SysV calling convention: a real, potentially-C, ABI-bound
// callee needs 16-byte stack alignment and correctly-placed stack-spilled
// arguments regardless of whether it was reached by name or through a
// pointer value. Splitting these into two separately-hand-rolled
// implementations (an earlier version of this fix did exactly that for the
// indirect case) is how a real, hard-to-diagnose bug got introduced — one
// tested implementation, parameterized on where the address comes from, is
// safer than two versions of the same non-trivial machinery.
static void emit_call_extern_va(JITBuffer* buf, void* fnaddr, const char* symbol_name, int vararg_float_count, int stack_arg_bytes, bool addr_in_rax) {
    if (addr_in_rax) {
        // mov r10, rax  ; save the already-computed callee address before
        // rax becomes scratch for the alignment/copy sequence below. r10 is
        // now LIVE (holds the callee address) for the rest of this function
        // — the padding/copy logic below must use a DIFFERENT scratch
        // register in this case (rax, freed up by this same move) instead
        // of r10 (used when !addr_in_rax, where r10 is genuinely free until
        // the address load happens further down).
        emit_byte(buf,0x49);emit_byte(buf,0x89);emit_byte(buf,0xc2);
    }
    // mov r11, rsp                ; preserve original rsp
    emit_byte(buf,0x49);emit_byte(buf,0x89);emit_byte(buf,0xe3);
    if (stack_arg_bytes == 0) {
        // Original, simpler path: nothing already sits at [rsp], so it's safe
        // to align rsp directly and discard whatever fell below it.
        // and rsp, -16                ; align down to 16
        emit_byte(buf,0x48);emit_byte(buf,0x83);emit_byte(buf,0xe4);emit_byte(buf,0xf0);
        // push r11 ; push r11         ; store orig rsp AND keep 16-alignment (two 8-byte pushes)
        emit_byte(buf,0x41);emit_byte(buf,0x53);
        emit_byte(buf,0x41);emit_byte(buf,0x53);
    } else if (addr_in_rax) {
        // Indirect call (through a fn-ptr value) with stack-spilled
        // arguments — the callee address is in r10 (moved there above,
        // before rax became scratch). Same alignment shape as the
        // !addr_in_rax branch below, but using rax as the padding/index
        // register throughout (r10 is live here, holding the callee
        // address, so it can't double as scratch the way it does below).
        //   mov rax, rsp ; and rax, 15   ; padding = rsp mod 16
        emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xc4);
        emit_byte(buf,0x48);emit_byte(buf,0x83);emit_byte(buf,0xe0);emit_byte(buf,0x0f);
        //   sub rsp, rax
        emit_byte(buf,0x48);emit_byte(buf,0x29);emit_byte(buf,0xc4);
        // push r11 ; push r11
        emit_byte(buf,0x41);emit_byte(buf,0x53);
        emit_byte(buf,0x41);emit_byte(buf,0x53);
        // Copy the payload via r12 (push/pop-preserved: nothing in this
        // function relies on r12 across the eventual call, so it's safe to
        // clobber as scratch here). rax holds padding (needed as the SIB
        // index every iteration), r10 the callee address, r11 saved rsp,
        // rdi/rsi/rdx/rcx/r8/r9 the six SysV argument registers already
        // correctly loaded — none of those are available as scratch.
        //
        // Both mov instructions below use mod=10 (disp32) ModRM encoding
        // (0xa4, not 0x64/mod=01/disp8) to match the 4-byte displacement
        // written via emit_u32 — this exact mismatch (mod=01 ModRM paired
        // with a 4-byte emit_u32 write) was a real bug that desynced the
        // instruction stream and caused a segfault a few bytes later,
        // found by single-stepping the actual compiled output in gdb and
        // reading the disassembly at the crash site directly, after
        // extensive attempts to find it through hand-derivation alone had
        // failed. A second, separate bug in the same investigation — REX.X
        // wrongly set to 1, extending the SIB index register from rax to
        // r8 and using a live argument value as a garbage stack offset —
        // was also only found this way, visible immediately in gdb's
        // register dump and disassembly where hours of manual tracing had
        // not surfaced it.
        emit_byte(buf,0x41);emit_byte(buf,0x54); // push r12
        for (int off = 0; off < stack_arg_bytes; off += 8) {
            // mov r12, [rsp + rax*1 + (16+off+8)]  (+8 for the push r12 above)
            emit_byte(buf,0x4c);emit_byte(buf,0x8b);emit_byte(buf,0xa4);emit_byte(buf,0x04);
            emit_u32(buf, (uint32_t)(off + 16 + 8));
            // mov [rsp + off + 8], r12
            emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0xa4);emit_byte(buf,0x24);
            emit_u32(buf, (uint32_t)(off + 8));
        }
        emit_byte(buf,0x41);emit_byte(buf,0x5c); // pop r12
    } else {
        // stack_arg_bytes worth of System V stack-argument payload already
        // sits at [rsp, rsp+stack_arg_bytes), positioned by the caller relative
        // to the CURRENT rsp so that the callee reads its 7th+ integer/pointer
        // argument at exactly [rsp+0], [rsp+8], ... at the moment of `call`.
        // `and rsp, -16` here would move rsp DOWN by up to 15 bytes, landing
        // it INSIDE that already-placed payload instead of below it — every
        // arg beyond the 6 register slots would then be read from the wrong
        // offset (this was the actual bug: a's..f's were correct since those
        // travel in registers unaffected by rsp, but g/h/... silently read
        // whatever garbage ended up at the shifted offset).
        //
        // Fix: insert alignment padding and the two rsp-save words BELOW the
        // stack-arg region (r10 is free here — the callee address load into
        // r10 happens after this, or was already moved there above if
        // addr_in_rax), THEN explicitly copy the stack-arg payload down to
        // sit at the new top of stack. The payload can't simply stay where
        // it is and have the new rsp coincide with it: the two rsp-save
        // pushes below necessarily land BETWEEN the new rsp and the
        // payload's fixed physical address (verified by tracing the actual
        // byte offsets — the payload ends up sitting at [new_rsp+padding+16],
        // not [new_rsp+0], no matter how the padding itself is computed), so
        // an explicit copy is required, not just careful arithmetic.
        //   mov r10, rsp                ; copy rsp so we can probe alignment
        emit_byte(buf,0x4c);emit_byte(buf,0x8b);emit_byte(buf,0xd4);
        //   and r10, 15                 ; r10 = rsp mod 16 = padding needed
        emit_byte(buf,0x49);emit_byte(buf,0x83);emit_byte(buf,0xe2);emit_byte(buf,0x0f);
        //   sub rsp, r10
        emit_byte(buf,0x4c);emit_byte(buf,0x29);emit_byte(buf,0xd4);
        // push r11 ; push r11
        emit_byte(buf,0x41);emit_byte(buf,0x53);
        emit_byte(buf,0x41);emit_byte(buf,0x53);
        // Copy the stack_arg_bytes payload from its original location
        // (still-unmoved physical address = current new-rsp + r10(padding) +
        // 16) down to [rsp+0, rsp+stack_arg_bytes). r10 currently holds the
        // padding amount from above — reuse it to compute the source offset
        // before clobbering it per 8-byte chunk. rax is free scratch (no
        // argument value is live in a GP register across this point; all six
        // register-bound args were already loaded into their SysV registers
        // before this function was ever called, and rax is not one of them).
        for (int off = 0; off < stack_arg_bytes; off += 8) {
            // mov rax, [rsp + r10 + 16 + off]   (source: original payload location)
            // Encoded as [rsp + r10] with a displacement via SIB addressing:
            // base=rsp, index=r10, scale=1, disp32=(16+off).
            emit_byte(buf,0x4a);emit_byte(buf,0x8b);emit_byte(buf,0x84);emit_byte(buf,0x14);
            emit_u32(buf, (uint32_t)(16 + off));
            // mov [rsp + off], rax               (destination: new top-of-stack slot)
            emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x84);emit_byte(buf,0x24);
            emit_u32(buf, (uint32_t)off);
        }
    }
    
    if (g_aot_mode && symbol_name) {
        if (vararg_float_count >= 0) {
            emit_byte(buf, 0xb0); emit_byte(buf, (uint8_t)vararg_float_count); // mov al, count
        }
        // call rel32
        emit_byte(buf, 0xe8);
        ELF_AddRelocation(buf->size, ELF_RELOC_EXTERN, symbol_name, 0);
        emit_u32(buf, 0);
    } else {
        if (!addr_in_rax) {
            // Load the callee address into r10, NOT rax, and call THROUGH r10.
            // al is rax's low byte — any 8-byte `mov rax, imm64` for the address
            // clobbers it regardless of ordering, and any `mov al, n` after the
            // address load corrupts the address's low byte. The two values need
            // genuinely separate registers; there is no ordering of "set al" and
            // "load address into rax" that preserves both, since they're the same
            // register. (Confirmed by two separate wrong attempts before this one:
            // al-after-address corrupted the call target by N bytes, landing
            // execution mid-instruction inside printf and faulting; al-before-
            // address got clobbered right back to 0 by the subsequent full 8-byte
            // address load, which is why every float silently read as 0.0 again.)
            emit_byte(buf,0x49);emit_byte(buf,0xba);emit_u64(buf,(uint64_t)fnaddr); // mov r10, fnaddr
        }
        // If addr_in_rax, r10 already holds the callee address — moved there
        // as the very first instruction this function emits, before rax
        // became scratch for the alignment/copy sequence above.
        if (vararg_float_count >= 0) {
            emit_byte(buf, 0xb0); emit_byte(buf, (uint8_t)vararg_float_count); // mov al, count
        }
        emit_byte(buf,0x41);emit_byte(buf,0xff);emit_byte(buf,0xd2); // call r10
    }
    // The callee may clobber r11, so we recover orig rsp from the stack slot we pushed.
    // After the call, rsp is back where it was right after our two pushes. Pop both:
    // pop r11 (discard) ; pop rsp-value into rsp via r11
    emit_byte(buf,0x41);emit_byte(buf,0x5b); // pop r11  (this is the 2nd pushed copy)
    emit_byte(buf,0x5c);                     // pop rsp  (restore original rsp directly)
}

// Non-vararg convenience wrapper, unchanged call shape for every existing
// (non-floating-point-vararg) call site.
static void emit_call_extern(JITBuffer* buf, void* fnaddr, const char* symbol_name) {
    emit_call_extern_va(buf, fnaddr, symbol_name, -1, 0, false);
}

// Store the low `w` bytes of rcx to [rax].
static void emit_store_to_rax(JITBuffer* buf, int w) {
    switch (w) {
        case 1: emit_byte(buf, 0x88); emit_byte(buf, 0x08); break;                      // mov [rax], cl
        case 2: emit_byte(buf, 0x66); emit_byte(buf, 0x89); emit_byte(buf, 0x08); break; // mov [rax], cx
        case 4: emit_byte(buf, 0x89); emit_byte(buf, 0x08); break;                      // mov [rax], ecx
        default: emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0x08); break; // mov [rax], rcx
    }
}

// Float-aware scalar store: value (f64 bits) in rcx, destination address in rax.
// For f32, genuinely round to single precision (cvtsd2ss) and store 4 bytes, so
// an f32 is a real 4-byte single — not f64's low half. f64 and integers store as
// their width. Returns true if it handled a float (caller then skips the int store).
// At a typed store boundary (decl/assign), if the destination is float but the value
// being stored came from an INTEGER source, convert it (rcx int -> rcx f64 bits) so
// `f64 x = 1` stores 1.0, not the raw integer bit-pattern. This honors the coercion
// that assignability already permits (int literal / int value -> float). dst float +
// src float = already f64 bits, no-op. Source non-float incl. pointer is treated as
// int-to-float here only when dst is float (assignability blocks ptr->float anyway).
static void emit_coerce_rcx_int_to_float(JITBuffer* buf, Type* dst, Type* src) {
    bool dst_f = Type_IsFloat(dst);
    bool src_f = Type_IsFloat(src);
    if (dst_f && !src_f) {
        // cvtsi2sd xmm0, rcx (F2 48 0F 2A C1) ; movq rcx, xmm0 (66 48 0F 7E C1)
        emit_byte(buf,0xf2);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x2a);emit_byte(buf,0xc1);
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc1);
    }
}

static bool emit_store_scalar_float(JITBuffer* buf, Type* t) {
    if (!t || t->cls != TYPE_PRIMITIVE) return false;
    if (t->primitive == PRIM_F32) {
        // movq xmm0, rcx ; cvtsd2ss xmm0, xmm0 ; movd [rax], xmm0
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc1); // movq xmm0, rcx
        emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x5a);emit_byte(buf,0xc0);                    // cvtsd2ss xmm0, xmm0
        emit_byte(buf,0x66);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0x00);                    // movd [rax], xmm0
        return true;
    }
    if (t->primitive == PRIM_F64) {
        emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0x08); // mov [rax], rcx
        return true;
    }
    return false;
}

// Float-aware scalar load: address in rax -> value (f64 bits) in rax.
// For f32, load 4 bytes and widen to f64 (cvtss2sd) so the rax-as-f64 compute model
// holds. Returns true if it handled a float.
static bool emit_load_scalar_float(JITBuffer* buf, Type* t) {
    if (!t || t->cls != TYPE_PRIMITIVE) return false;
    if (t->primitive == PRIM_F32) {
        // movd xmm0, [rax] ; cvtss2sd xmm0, xmm0 ; movq rax, xmm0
        emit_byte(buf,0x66);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0x00);                    // movd xmm0, [rax]
        emit_byte(buf,0xf3);emit_byte(buf,0x0f);emit_byte(buf,0x5a);emit_byte(buf,0xc0);                    // cvtss2sd xmm0, xmm0
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0); // movq rax, xmm0
        return true;
    }
    if (t->primitive == PRIM_F64) {
        emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x00); // mov rax, [rax]
        return true;
    }
    return false;
}

// mov rsi, <sysv arg register #idx>  (RDI,RSI,RDX,RCX,R8,R9)
static void emit_mov_rsi_from_argreg(JITBuffer* buf, int idx) {
    switch (idx) {
        case 0: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xfe); break; // mov rsi, rdi
        case 1: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xf6); break; // mov rsi, rsi
        case 2: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xd6); break; // mov rsi, rdx
        case 3: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xce); break; // mov rsi, rcx
        case 4: emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0xc6); break; // mov rsi, r8
        case 5: emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0xce); break; // mov rsi, r9
    }
}

// mov [rbp - off], <sysv arg register #idx>
static void emit_spill_argreg_to_slot(JITBuffer* buf, int idx, int off) {
    uint32_t neg = (uint32_t)(-off);
    switch (idx) {
        case 0: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xbd);emit_u32(buf,neg); break; // rdi
        case 1: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xb5);emit_u32(buf,neg); break; // rsi
        case 2: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x95);emit_u32(buf,neg); break; // rdx
        case 3: emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x8d);emit_u32(buf,neg); break; // rcx
        case 4: emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0x85);emit_u32(buf,neg); break; // r8
        case 5: emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0x8d);emit_u32(buf,neg); break; // r9
    }
}

static void emit_sysv_arg_reg(JITBuffer* buf, int arg_idx) {
    // RDI, RSI, RDX, RCX, R8, R9
    // pop to register
    switch (arg_idx) {
        case 0: emit_byte(buf, 0x5f); break; // pop rdi
        case 1: emit_byte(buf, 0x5e); break; // pop rsi
        case 2: emit_byte(buf, 0x5a); break; // pop rdx
        case 3: emit_byte(buf, 0x59); break; // pop rcx
        case 4: emit_byte(buf, 0x41); emit_byte(buf, 0x58); break; // pop r8
        case 5: emit_byte(buf, 0x41); emit_byte(buf, 0x59); break; // pop r9
    }
}

// mov <sysv arg register #arg_idx>, [rsp + off]  (disp32 — no range limit, unlike
// disp8's 0..127; the register region's offset from rsp grows with how many
// stack-spilled args sit above it, so this must not silently truncate for calls
// with many arguments).
// Used instead of emit_sysv_arg_reg's pop when stack-spilled args (7th+) occupy
// the top of the stack — popping would consume the wrong region (the stack args,
// not the register-bound ones sitting below them). This reads by computed offset
// instead, leaving rsp untouched; the caller drops the whole register-arg region
// afterward with one rsp adjustment once every register has been loaded.
static void emit_mov_argreg_from_rsp_off(JITBuffer* buf, int arg_idx, int off) {
    switch (arg_idx) {
        case 0: emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xbc);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov rdi,[rsp+off]
        case 1: emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xb4);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov rsi,[rsp+off]
        case 2: emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x94);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov rdx,[rsp+off]
        case 3: emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x8c);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov rcx,[rsp+off]
        case 4: emit_byte(buf,0x4c);emit_byte(buf,0x8b);emit_byte(buf,0x84);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov r8,[rsp+off]
        case 5: emit_byte(buf,0x4c);emit_byte(buf,0x8b);emit_byte(buf,0x8c);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off); break; // mov r9,[rsp+off]
    }
}

// pop the top-of-stack 8 bytes directly into xmm register #idx (0..7), via rax
// as scratch (x86 has no pop-to-xmm instruction). Used for System V floating-
// point arguments, which travel in xmm0..xmm7 — a SEPARATE register file from
// the integer/pointer argument registers, with its own independent counter.
// This is the piece that was missing entirely: every float/double argument
// was previously placed in a GP register alongside integers, which a real
// variadic C callee (printf et al.) never looks at, since va_arg for a
// floating-point format specifier reads from the xmm save area, not GP regs.
static void emit_pop_to_xmm(JITBuffer* buf, int idx) {
    emit_byte(buf, 0x58); // pop rax
    switch (idx) {
        case 0: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0); break; // movq xmm0, rax
        case 1: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc8); break; // movq xmm1, rax
        case 2: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xd0); break; // movq xmm2, rax
        case 3: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xd8); break; // movq xmm3, rax
        case 4: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xe0); break; // movq xmm4, rax
        case 5: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xe8); break; // movq xmm5, rax
        case 6: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xf0); break; // movq xmm6, rax
        case 7: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xf8); break; // movq xmm7, rax
    }
}

// True if t is a floating-point primitive (f32 or f64) — the SysV check for
// "does this argument travel in an xmm register, not a GP one."


static void compile_node_ctx(JITBuffer* buf, ASTNode* node, LoopContext* loop);

// ── x64 LayoutSink ────────────────────────────────────────────────────────────
// Drive layout_fill with x86 stores. PRECONDITION for the whole fill: the
// destination base address is on top of the stack ([rsp]). enter_sub/leave_sub
// push/pop nested bases the same way the old hand-rolled walker did; put_*
// always reload the current base from [rsp] so it survives clobbers.
typedef struct {
    JITBuffer* buf;
    LoopContext* loop;
} X64LayoutCtx;

static void x64_load_base_plus_off(JITBuffer* buf, uint64_t off) {
    emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x04); emit_byte(buf, 0x24); // mov rax, [rsp]
    if (off) {
        emit_byte(buf, 0x48); emit_byte(buf, 0x05); emit_u32(buf, (uint32_t)off); // add rax, imm32
    }
}

static bool x64_sink_put_scalar(void* ctx, uint64_t offset, int width, bool is_float, ASTNode* val) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    compile_node_ctx(c->buf, val, c->loop);                     // value in rax
    emit_byte(c->buf, 0x48); emit_byte(c->buf, 0x89); emit_byte(c->buf, 0xc1); // mov rcx, rax
    x64_load_base_plus_off(c->buf, offset);
    if (is_float && width == 4) {
        // f32 store: round f64 bits in rcx down to single, write 4 bytes.
        emit_byte(c->buf,0x66);emit_byte(c->buf,0x48);emit_byte(c->buf,0x0f);emit_byte(c->buf,0x6e);emit_byte(c->buf,0xc1); // movq xmm0, rcx
        emit_byte(c->buf,0xf2);emit_byte(c->buf,0x0f);emit_byte(c->buf,0x5a);emit_byte(c->buf,0xc0);                    // cvtsd2ss xmm0, xmm0
        emit_byte(c->buf,0x66);emit_byte(c->buf,0x0f);emit_byte(c->buf,0x7e);emit_byte(c->buf,0x00);                    // movd [rax], xmm0
    } else if (is_float) {
        emit_byte(c->buf, 0x48); emit_byte(c->buf, 0x89); emit_byte(c->buf, 0x08); // mov [rax], rcx
    } else {
        emit_store_to_rax(c->buf, width);
    }
    return true;
}

static bool x64_sink_put_default(void* ctx, uint64_t offset, uint64_t size, uint8_t* bytes) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    uint64_t off = 0;
    while (off < size) {
        uint64_t rem = size - off;
        int w = (rem >= 8) ? 8 : (rem >= 4) ? 4 : (rem >= 2) ? 2 : 1;
        uint64_t chunk = 0;
        memcpy(&chunk, bytes + off, (size_t)w);
        emit_byte(c->buf, 0x48); emit_byte(c->buf, 0xb9); emit_u64(c->buf, chunk); // movabs rcx, imm
        x64_load_base_plus_off(c->buf, offset + off);
        emit_store_to_rax(c->buf, w);
        off += (uint64_t)w;
    }
    return true;
}

static bool x64_sink_put_agg_value(void* ctx, uint64_t offset, uint64_t size, ASTNode* val) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    // Push dest = base+offset, then memcpy from the aggregate value's address.
    x64_load_base_plus_off(c->buf, offset);
    emit_byte(c->buf, 0x50); // push dest
    compile_node_ctx(c->buf, val, c->loop);                                         // rax = &src
    emit_byte(c->buf, 0x48); emit_byte(c->buf, 0x89); emit_byte(c->buf, 0xc6);       // mov rsi, rax
    emit_byte(c->buf, 0x48); emit_byte(c->buf, 0x8b); emit_byte(c->buf, 0x3c); emit_byte(c->buf, 0x24); // mov rdi, [rsp]
    emit_memcpy_rdi_rsi(c->buf, size);
    emit_byte(c->buf, 0x58); // pop dest
    return true;
}

static bool x64_sink_put_tag(void* ctx, int tag) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    emit_byte(c->buf, 0x48); emit_byte(c->buf, 0xb9); emit_u64(c->buf, (uint64_t)tag); // movabs rcx, tag
    x64_load_base_plus_off(c->buf, 0);
    emit_store_to_rax(c->buf, 4); // u32 tag at offset 0
    return true;
}

static bool x64_sink_enter_sub(void* ctx, uint64_t offset) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    x64_load_base_plus_off(c->buf, offset);
    emit_byte(c->buf, 0x50); // push nested base
    return true;
}

static void x64_sink_leave_sub(void* ctx) {
    X64LayoutCtx* c = (X64LayoutCtx*)ctx;
    emit_byte(c->buf, 0x58); // pop nested base
}

// Fill an aggregate literal (struct or array) into a destination buffer.
// PRECONDITION: the destination base address is on top of the stack ([rsp]).
// The base stays on the stack; the caller pops it. Traversal lives in
// layout_fill (codegen.c); this is only the x86 sink.
static void emit_fill_literal(JITBuffer* buf, ASTNode* lit, LoopContext* loop) {
    if (lit->type == AST_STRUCT_LITERAL && !lit->struct_lit.sdef) {
        // Still-unresolved literal reaching codegen: no target ever gave it a
        // concrete type. Diagnose here rather than letting layout_fill bail
        // silently (it returns false on NULL sdef).
        Error_AtNode(lit, "compiler bug: unresolved literal reached emit_fill_literal", NULL);
    }
    X64LayoutCtx ctx = { .buf = buf, .loop = loop };
    LayoutSink sink = {
        .ctx = &ctx,
        .put_scalar = x64_sink_put_scalar,
        .put_default = x64_sink_put_default,
        .put_agg_value = x64_sink_put_agg_value,
        .put_tag = x64_sink_put_tag,
        .enter_sub = x64_sink_enter_sub,
        .leave_sub = x64_sink_leave_sub,
    };
    if (!layout_fill(lit, &sink)) {
        Error_AtNode(lit, "compiler bug: layout_fill failed in emit_fill_literal", NULL);
    }
}

// Narrow whatever's in rax down to `target`'s actual width/precision, in
// place (rax in, rax out). Unifies two previously-separate concerns that are
// really the same bug: integer ops compute at full 64-bit width in rax, and
// float ops compute at f64 precision in xmm0/xmm1 — either one leaves a
// too-wide/too-precise result in rax whenever the logical type is narrower
// (u8/i8/u16/i16, or f32). That's invisible whenever the value is immediately
// stored to a slot of the right size (the store truncates/rounds it as a
// side effect) but leaks through for any register-resident use with no
// intervening store or cast — return, a cast target, a nested sub-expression.
// Called once per binary-op result (int tail and float tail below) instead of
// duplicating narrow/round logic at every register-resident use site.
static void emit_narrow_rax_to_type(JITBuffer* buf, Type* target) {
    if (!target) return;
    if (target->cls == TYPE_PRIMITIVE && target->primitive == PRIM_F32) {
        // The rest of the backend's convention (emit_store_scalar_float,
        // emit_load_scalar_float, emit_coerce_rcx_int_to_float) is that a
        // float value carried in rax/rcx between expressions is ALWAYS f64
        // bits, even when its logical type is f32 — f32 only becomes a real
        // 4-byte value at the store boundary, where it's rounded down AND
        // narrowed to 4 bytes together. So here we must round to f32
        // precision but widen back to f64 bits (cvtss2sd), not truncate to
        // eax — leaving raw f32 bits in rax would corrupt the next
        // consumer, which will reinterpret rax as a double.
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0); // movq xmm0, rax
        emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x5a);emit_byte(buf,0xc0);                    // cvtsd2ss xmm0, xmm0 (round to f32)
        emit_byte(buf,0xf3);emit_byte(buf,0x0f);emit_byte(buf,0x5a);emit_byte(buf,0xc0);                    // cvtss2sd xmm0, xmm0 (widen back to f64 bits)
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0); // movq rax, xmm0
        return;
    }
    if (Type_IsFloat(target)) return; // f64: already full precision, nothing to narrow
    int w = Type_Width(target);
    bool sgn = Type_IsSigned(target);
    switch (w) {
        case 1:
            if (sgn) { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xbe);emit_byte(buf,0xc0); } // movsx rax, al
            else     { emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xc0); }                     // movzx eax, al
            break;
        case 2:
            if (sgn) { emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xbf);emit_byte(buf,0xc0); } // movsx rax, ax
            else     { emit_byte(buf,0x0f);emit_byte(buf,0xb7);emit_byte(buf,0xc0); }                     // movzx eax, ax
            break;
        case 4:
            if (sgn) { emit_byte(buf,0x48);emit_byte(buf,0x63);emit_byte(buf,0xc0); }                     // movsxd rax, eax
            else     { emit_byte(buf,0x89);emit_byte(buf,0xc0); }                                         // mov eax, eax
            break;
        default: break; // 8 bytes: already full width, nothing to do
    }
}

static void compile_node_ctx(JITBuffer* buf, ASTNode* node, LoopContext* loop);

static void emit_defers(JITBuffer* buf, DeferCtx* target, LoopContext* loop) {
    for (DeferCtx* d = s_defer_stack; d != target; d = d->next) {
        compile_node_ctx(buf, d->stmt, loop);
    }
}

static void compile_node_ctx(JITBuffer* buf, ASTNode* node, LoopContext* loop) {
    if (!node) return;

    // Aggregate literal as an rvalue (e.g. a call argument): materialize into a
    // STABLE rbp-relative frame slot and yield its address. We must NOT use a
    // transient `sub rsp / add rsp` temp here: the caller pushes the returned
    // address and keeps evaluating (more pushes, more aggregate temps), and the
    // callee only memcpys the by-value arg out of this pointer at call time. A
    // temp reclaimed when this function returns would be a dangling pointer into
    // reclaimed stack — the bug that made `f({.x=1})` / `f(.Variant{..})` yield
    // garbage. A frame slot is allocated once per source occurrence and reused
    // across loop iterations, so there is no per-iteration stack growth either.
    if (node->type == AST_STRUCT_LITERAL || node->type == AST_ARRAY_LITERAL) {
        Type* t = Type_Infer(node);
        if (!t) {
            Error_AtNode(node, "compiler bug: unresolved literal reached backend", NULL);
        }
        uint64_t sz = Type_SizeOf(t);
        uint64_t slot = (sz + 15) & ~(uint64_t)15;
        s_func->current_local_offset += (int)slot;
        int off = s_func->current_local_offset;          // temp lives at [rbp - off]
        // dest base -> rax : lea rax, [rbp - off]
        emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x85); emit_u32(buf, (uint32_t)(-off));
        emit_byte(buf, 0x50);                                            // push dest base (rax)
        // zero the temp (rax already = base)
        for (uint64_t b = 0; b < sz; b += 8) {
            emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0x80); emit_u32(buf, (uint32_t)b); emit_u32(buf, 0);
        }
        emit_fill_literal(buf, node, loop);
        emit_byte(buf, 0x58); // pop dest base -> rax (the temp's address = the value)
        return;
    }

    if (node->type == AST_BLOCK) {
        DeferCtx* old_stack = s_defer_stack;
        for (size_t i = 0; i < node->block.count; i++) {
            if (node->block.statements[i]->type == AST_DEFER) {
                DeferCtx* d = (DeferCtx*)malloc(sizeof(DeferCtx));
                d->stmt = node->block.statements[i]->unary;
                d->next = s_defer_stack;
                s_defer_stack = d;
            } else {
                compile_node_ctx(buf, node->block.statements[i], loop);
            }
        }
        while (s_defer_stack != old_stack) {
            compile_node_ctx(buf, s_defer_stack->stmt, loop);
            DeferCtx* old = s_defer_stack;
            s_defer_stack = s_defer_stack->next;
            free(old);
        }
        return;
    }
    
    if (node->type == AST_FUNC_DECL) {
        // A generic function (e.g. an impl method sharing the struct's T) is never
        // compiled at its definition site — only monomorphized instantiations are,
        // queued and compiled on demand in Backend_Finalize. Top-level generic fns
        // are gated in Backend_Compile; this mirrors that for ones nested in a block
        // (impl desugars its methods into a block of sibling AST_FUNC_DECLs).
        if (node->func_decl.type_param_count > 0) return;

        emit_byte(buf, 0xe9);
        size_t jmp_over_offset = buf->size; emit_u32(buf, 0);

        Symbol* sym = node->func_decl.sym;
        if (sym) sym->offset = buf->size;

        // Prologue
        emit_byte(buf, 0x55); // push rbp
        emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xe5); // mov rbp, rsp
        emit_byte(buf, 0x41); emit_byte(buf, 0x57); // push r15
        emit_byte(buf, 0x48); emit_byte(buf, 0x81); emit_byte(buf, 0xec); // sub rsp, ...
        size_t sub_rsp_offset = buf->size; 
        emit_u32(buf, 0); // placeholder

        // Open a return context for this function (saves the enclosing one).
        FuncContext fc = {0};
        fc.current_local_offset = 8; // slot 0 is saved r15

        // Aggregate return uses the sret ABI: a hidden pointer to caller-allocated
        // result space arrives in arg-register 0, shifting real params up by one.
        Type* rtype = node->func_decl.return_type;
        bool agg_ret = Type_IsAggregate(rtype);
        int arg_base = agg_ret ? 1 : 0;
        int sret_slot = 0;
        if (agg_ret) {
            sret_slot = fc.current_local_offset;
            fc.current_local_offset += 8;
            // Spill the hidden result pointer (arg reg 0 = rdi) to its slot.
            emit_spill_argreg_to_slot(buf, 0, sret_slot);
        }

        fc.ret_type = rtype;
        fc.aggregate_ret = agg_ret;
        fc.sret_slot = sret_slot;
        FuncContext* prev_func = s_func;
        s_func = &fc;

        // Recalculate parameter offsets (essential for generic instantiations where sizes grew).
        for (size_t i = 0; i < node->func_decl.param_count; i++) {
            Symbol* p = node->func_decl.param_syms[i];
            if (p) {
                uint64_t sz = Type_SizeOf(p->type);
                uint64_t slot = (sz + 7) & ~7ULL;
                s_func->current_local_offset += slot;
                p->offset = s_func->current_local_offset;
            }
        }

        // Spill args to stack, into each param's real symbol slot. Params whose
        // areg index lands in [0,6) arrive in a register; params at or past that
        // arrived on the STACK (System V: 7th+ integer/pointer arg), placed by the
        // call-site's in-place reversal so the lowest stack-arg index sits closest
        // to the return address — i.e. at [rbp+16], then [rbp+24], etc. (rbp+0 =
        // saved rbp, rbp+8 = return address, per this function's own prologue).
        for (size_t i = 0; i < node->func_decl.param_count; i++) {
            Symbol* p = node->func_decl.param_syms[i];
            Type* pt = p ? p->type : NULL;
            int areg = arg_base + (int)i;
            bool from_stack = areg >= 6;
            int stack_off = from_stack ? (16 + (areg - 6) * 8) : 0;
            if (Type_IsAggregate(pt)) {
                // Aggregate param: the arg slot holds a POINTER to the caller's copy
                // (register OR stack, same convention either way). memcpy those bytes
                // into this param's own local slot (true by-value).
                emit_byte(buf, 0x57); emit_byte(buf, 0x56); emit_byte(buf, 0x51);
                if (from_stack) {
                    // mov rsi, [rbp+stack_off]
                    emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xb5);emit_u32(buf,(uint32_t)stack_off);
                } else {
                    emit_mov_rsi_from_argreg(buf, areg);
                }
                emit_byte(buf,0x48);emit_byte(buf,0x8d);emit_byte(buf,0xbd);
                emit_u32(buf, (uint32_t)(-p->offset));               // lea rdi, [rbp-off]
                emit_memcpy_rdi_rsi(buf, Type_SizeOf(pt)); emit_byte(buf, 0x59); emit_byte(buf, 0x5e); emit_byte(buf, 0x5f);
            } else if (from_stack) {
                int dst_off = p ? p->offset : (int)((i + 1) * 8);
                // mov rax, [rbp+stack_off] ; mov [rbp-dst_off], rax
                emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x85);emit_u32(buf,(uint32_t)stack_off);
                emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x85);emit_u32(buf,(uint32_t)(-dst_off));
            } else {
                emit_spill_argreg_to_slot(buf, areg, p ? p->offset : (int)((i + 1) * 8));
            }
        }

        compile_node_ctx(buf, node->func_decl.body, loop);

        // Epilogue label: every `return` jumps here.
        for (size_t i = 0; i < fc.ret_count; i++) {
            size_t off = fc.ret_offsets[i];
            *(uint32_t*)(buf->code + off) = buf->size - (off + 4);
        }
        free(fc.ret_offsets);
        s_func = prev_func;

        // Patch the stack allocation size (16-byte aligned)
        uint32_t final_stack_size = (fc.current_local_offset + 15) & ~15;
        *(uint32_t*)(buf->code + sub_rsp_offset) = final_stack_size;

        emit_byte(buf, 0x4c); emit_byte(buf, 0x8b); emit_byte(buf, 0x7d); emit_byte(buf, 0xf8); // mov r15, [rbp-8]
        emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xec); // mov rsp, rbp
        emit_byte(buf, 0x5d); // pop rbp
        emit_byte(buf, 0xc3); // ret

        *(uint32_t*)(buf->code + jmp_over_offset) = buf->size - (jmp_over_offset + 4);
        emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xc0); // xor rax, rax
        return;
    }

    if (node->type == AST_RETURN) {
        if (!s_func) {
            Error_AtNode(node, "return outside of function", NULL);
        }
        if (node->unary) {
            compile_node_ctx(buf, node->unary, loop); // scalar: value in rax; aggregate: &source in rax
            if (s_func->aggregate_ret) {
                // memcpy the aggregate into the caller's result space, return that pointer.
                emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xc6); // mov rsi, rax (src)
                // rdi = [rbp - sret_slot]  (the hidden result pointer)
                emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0xbd);
                emit_u32(buf, (uint32_t)(-s_func->sret_slot));
                emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xf8); // mov rax, rdi
                emit_memcpy_rdi_rsi(buf, Type_SizeOf(s_func->ret_type));
            }
            
            Type* rt = s_func->ret_type;
            if (rt && Type_IsFloat(rt)) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x83); emit_byte(buf, 0xec); emit_byte(buf, 0x08); // sub rsp, 8
                if (rt->primitive == PRIM_F32) { emit_byte(buf, 0xf3); } else { emit_byte(buf, 0xf2); }
                emit_byte(buf, 0x0f); emit_byte(buf, 0x11); emit_byte(buf, 0x04); emit_byte(buf, 0x24); // movss/sd [rsp], xmm0
            } else {
                emit_byte(buf, 0x50); // push rax
            }
        }
        
        emit_defers(buf, NULL, loop);
        
        if (node->unary) {
            Type* rt = s_func->ret_type;
            if (rt && Type_IsFloat(rt)) {
                if (rt->primitive == PRIM_F32) { emit_byte(buf, 0xf3); } else { emit_byte(buf, 0xf2); }
                emit_byte(buf, 0x0f); emit_byte(buf, 0x10); emit_byte(buf, 0x04); emit_byte(buf, 0x24); // movss/sd xmm0, [rsp]
                emit_byte(buf, 0x48); emit_byte(buf, 0x83); emit_byte(buf, 0xc4); emit_byte(buf, 0x08); // add rsp, 8
            } else {
                emit_byte(buf, 0x58); // pop rax
            }
        }

        emit_byte(buf, 0xe9);
        func_add_ret(s_func, buf->size);
        emit_u32(buf, 0);
        return;
    }
    
    if (node->type == AST_NEW) {
        // Compute the byte size — ALWAYS via Type_SizeOf, never inspecting the type's
        // kind, so this composes with nested aggregates and (later) generic structs.
        Type* et = node->new_expr.alloc_type;
        uint64_t elem_size = Type_SizeOf(et);

        // The byte count is needed twice (once to size the malloc, once to size the
        // zeroing memset below) but must only be EVALUATED once: `count` can have
        // side effects (new[n()] u8), and a stack push doesn't survive the malloc
        // call either (emit_call_extern_va's zero-stack-arg path does `and rsp,-16`
        // and discards whatever was below rsp). So spill the computed byte count to
        // a stable rbp-relative frame slot instead, which the call can't disturb.
        int count_slot = 0;
        if (node->new_expr.count) {
            // new T[expr]: size = count * elem_size. Evaluate count -> rax, multiply.
            compile_node_ctx(buf, node->new_expr.count, loop); // rax = count (evaluated exactly once)
            emit_byte(buf,0x48);emit_byte(buf,0xbf);emit_u64(buf,elem_size); // mov rdi, elem_size
            emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xaf);emit_byte(buf,0xc7); // imul rax, rdi

            s_func->current_local_offset += 8;
            count_slot = s_func->current_local_offset;
            emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x85);emit_u32(buf,(uint32_t)(-count_slot)); // mov [rbp-slot], rax

            emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xc7); // mov rdi, rax (size arg)
        } else {
            // new T or new T{...}: size = elem_size.
            emit_byte(buf,0x48);emit_byte(buf,0xbf);emit_u64(buf,elem_size); // mov rdi, elem_size
        }
        emit_call_extern(buf, (void*)malloc, "malloc"); // rax = allocated pointer
        // Zero the allocation (new always zero-inits). Save ptr in r12 (callee-saved),
        // memset via rep stosb, then restore the pointer to rax.
        emit_byte(buf,0x41);emit_byte(buf,0x54);                 // push r12
        emit_byte(buf,0x49);emit_byte(buf,0x89);emit_byte(buf,0xc4); // mov r12, rax (save ptr)
        if (node->new_expr.count) {
            emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x8d);emit_u32(buf,(uint32_t)(-count_slot)); // mov rcx, [rbp-slot] (byte count, no re-eval)
        } else {
            emit_byte(buf,0x48);emit_byte(buf,0xb9);emit_u64(buf,elem_size); // mov rcx, elem_size
        }
        emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0xe7); // mov rdi, r12 (dest)
        emit_byte(buf,0x30);emit_byte(buf,0xc0);                     // xor al, al (fill byte 0)
        emit_byte(buf,0xf3);emit_byte(buf,0xaa);                     // rep stosb
        emit_byte(buf,0x4c);emit_byte(buf,0x89);emit_byte(buf,0xe0); // mov rax, r12 (ptr back)
        emit_byte(buf,0x41);emit_byte(buf,0x5c);                     // pop r12

        // new T{...}: fill the just-allocated object from the initializer literal.
        if (node->new_expr.init) {
            emit_byte(buf,0x50); // push dest (the allocation pointer in rax)
            emit_fill_literal(buf, node->new_expr.init, loop);
            emit_byte(buf,0x58); // pop dest (restore pointer to rax)
        }
        return;
    }

    if (node->type == AST_DELETE) {
        compile_node_ctx(buf, node->delete_expr.ptr, loop); // rax = pointer
        emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xc7); // mov rdi, rax
        emit_call_extern(buf, (void*)free, "free");
        return;
    }

    if (node->type == AST_CALL) {
        Type* crt = NULL;
        bool agg_ret = false;

        bool is_vararg = false;
        bool is_extern_call = false; // gates SysV float/xmm routing — see use below
        Type* call_tgt = NULL;
        // A `target_expr` call whose target's type resolved to TYPE_FN_LITERAL
        // (e.g. `cmp(a,b)` where `cmp`'s generic param was bound to a specific
        // function like `asc`) is knowable at COMPILE time -- there is exactly
        // one possible callee, the same as an ordinary direct call, just spelled
        // through a local variable instead of a bare name. Route it through the
        // direct-call symbol path below (skips loading a runtime fnptr value and
        // calling indirectly through it) instead of falling into the generic
        // indirect-call codegen at the bottom of this function.
        Symbol* direct_lit_sym = NULL;
        if (node->call.target_expr) {
            Type* raw_tgt = Type_Infer(node->call.target_expr);
            // Only take the direct-call shortcut when the target expression is a
            // bare identifier (AST_IDENT) -- i.e. no side effects to preserve by
            // evaluating it. A TYPE_FN_LITERAL reached through anything more
            // complex (a field, an index, another call) still goes through the
            // ordinary indirect path below; correctness over cleverness.
            if (raw_tgt && raw_tgt->cls == TYPE_FN_LITERAL && raw_tgt->fn_lit.sym &&
                (node->call.target_expr->type == AST_IDENT || node->call.target_expr->type == AST_CAST)) {
                direct_lit_sym = raw_tgt->fn_lit.sym;
            } else if (raw_tgt && raw_tgt->cls == TYPE_FUNCTION && node->call.target_expr->type == AST_CAST &&
                       node->call.target_expr->cast.expr && node->call.target_expr->cast.expr->type == AST_INT_LITERAL) {
                Symbol* sym = (Symbol*)(intptr_t)node->call.target_expr->cast.expr->int_value;
                if (sym && sym->kind == SYM_FUNCTION) {
                    direct_lit_sym = sym;
                }
            }
            call_tgt = Type_FnLitShape(raw_tgt);
            if (call_tgt && call_tgt->cls == TYPE_POINTER && call_tgt->pointer_base && call_tgt->pointer_base->cls == TYPE_FUNCTION) {
                call_tgt = call_tgt->pointer_base;
            }
            if (!call_tgt || call_tgt->cls != TYPE_FUNCTION) {
                Error_AtNode(node, "compiler bug: calling non-function reached codegen", NULL);
            }
            crt = call_tgt->function.return_type;
            is_vararg = call_tgt->function.is_vararg;
            // Previously: "fnptrs can only ever point at Torrent functions
            // today (no way to take the address of an extern symbol as a
            // first-class fnptr value anywhere in the grammar)" — that
            // invariant no longer holds (SYM_FUNCTION-in-value-position now
            // resolves an extern function's real address instead of
            // computing a meaningless JIT-buffer-relative offset for it), and
            // there's no `sym` here to check `is_extern` against even if we
            // wanted to — an indirect call only has the fn-ptr's VALUE, not
            // which symbol originally produced it. Can't prove the target
            // isn't extern at compile time, so the SysV stack-alignment
            // protection below is now applied unconditionally for indirect
            // calls, matching what emit_call_extern_va already does for
            // direct extern calls, rather than silently risking the same
            // class of ABI-alignment bug this session already found and
            // fixed once for the direct-call path.
        } else {
            Symbol* sym = node->call.sym;
            if (!sym || sym->kind != SYM_FUNCTION) {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf),
                         "unresolved function call: sym=%s, target_expr_type=%d, target_name=%.*s",
                         sym ? sym->name : "NULL symbol",
                         node->call.target_expr ? (int)node->call.target_expr->type : -1,
                         node->call.target_name ? (int)node->call.target_name_len : 4,
                         node->call.target_name ? node->call.target_name : "NONE");
                Error_AtNode(node, errbuf, NULL);
            }
            is_extern_call = sym->is_extern;
            call_tgt = sym->type;
            // For a generic instantiation, the return type comes from the
            // substituted signature, not the generic's TYPE_PARAM signature.
            if (node->call.type_arg_count > 0 && sym->generic_decl) {
                Symbol* isym = Generic_Instantiate(sym, node->call.type_args, node->call.type_arg_count);
                call_tgt = isym->type;
            }
            if (call_tgt && call_tgt->cls == TYPE_FUNCTION) {
                crt = call_tgt->function.return_type;
                is_vararg = call_tgt->function.is_vararg;
            } else {
                crt = call_tgt; // fallback
            }
        }
        agg_ret = Type_IsAggregate(crt);

        // For aggregate return (sret): allocate result space on the caller stack and
        // pass its address as hidden arg-register 0 (rdi); real args shift up by one.
        // We stash the result address in RBX, which is callee-saved (preserved across
        // the call) and unused elsewhere by this backend.
        // Snapshot rsp so we can fully reclaim arg-copy scratch right after the call.
        // r14 holds the snapshot, rbx the sret result pointer; both callee-saved, and
        // pushed/popped to survive nested calls.
        emit_byte(buf, 0x41); emit_byte(buf, 0x56);                     // push r14
        emit_byte(buf, 0x53);                                          // push rbx
        emit_byte(buf, 0x49); emit_byte(buf, 0x89); emit_byte(buf, 0xe6); // mov r14, rsp (snapshot)

        if (agg_ret) {
            // The sret result must outlive the arg scratch we reclaim after the
            // call, so it needs a STABLE rbp-relative frame slot rather than an
            // rsp temp.
            //
            // This used to be a single hardcoded `lea rbx, [rbp-1024]` — one
            // fixed scratch region shared by EVERY aggregate-returning call in
            // the function. That is wrong two ways, and both bit hard:
            //
            //  1. Two struct-returning calls live at once (`f(g(), h())`, or the
            //     self-hosted backend's `e.binop("mul", bi, val_lit(..))`, where
            //     val_lit returns a struct by value) both target rbp-1024, so the
            //     second silently overwrites the first's result.
            //
            //  2. -1024 is outside current_local_offset's accounting entirely, so
            //     in any function whose locals exceed 1024 bytes it ALIASES a real
            //     local. Small functions got away with it (rbp-1024 landed in dead
            //     frame space); big ones — compile_expr in the LLVM backend, say —
            //     had their locals scribbled on. That is the whole "struct returned
            //     by value comes back corrupt, but only in large functions" class
            //     of bug, and the source-level workarounds for it (hoisting values
            //     into named locals to dodge the clobber) are scattered all over
            //     the self-hosted backend.
            //
            // Allocate a real slot instead, exactly as the aggregate-literal case
            // above does: bump current_local_offset by the aligned size so the slot
            // is accounted for in the frame, and is distinct per call site. Slots
            // are allocated once per source occurrence and reused across loop
            // iterations, so there is no per-iteration stack growth.
            uint64_t rsz = Type_SizeOf(crt);
            uint64_t rslot = (rsz + 15) & ~(uint64_t)15;
            if (rslot == 0) rslot = 16;
            s_func->current_local_offset += (int)rslot;
            int roff = s_func->current_local_offset;
            emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x9d);
            emit_u32(buf, (uint32_t)(-roff)); // lea rbx, [rbp - roff]
        }

        // Evaluate args left-to-right; each pushes the value destined for its register
        // OR the stack (System V: args beyond the 6 integer/pointer registers go on
        // the stack). Aggregate values naturally evaluate to their address (pointer),
        // which we simply push. The callee is responsible for memcpy'ing from this
        // pointer into its own stack frame to achieve true by-value semantics.
        //
        // Side effects must stay observable in left-to-right source order, so this
        // loop's evaluation order is unchanged from the <=6-arg case. What changes is
        // what happens to the stack AFTER evaluation: with N>6 args, the top (N-6)
        // stack slots (args 6..N-1, in REVERSE index order — arg_{N-1} ends up on top,
        // since it was pushed last) need to be reversed in place so the LOWEST-indexed
        // stack arg (arg6) ends up closest to the eventual return address, matching
        // where the callee's positive [rbp+16], [rbp+24], ... reads expect it.
        for (size_t i = 0; i < node->call.arg_count; i++) {
            compile_node_ctx(buf, node->call.args[i], loop);
            
            if (call_tgt && call_tgt->cls == TYPE_FUNCTION && i < call_tgt->function.param_count) {
                Type* param_t = call_tgt->function.param_types[i];
                if (param_t && param_t->cls == TYPE_PRIMITIVE && param_t->primitive == PRIM_F32) {
                    Type* arg_t = Type_Infer(node->call.args[i]);
                    if (Type_IsFloat(arg_t)) {
                        // Narrow from f64 (rax) to f32 bits (rax).
                        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0); // movq xmm0, rax
                        emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x5a);emit_byte(buf,0xc0);                    // cvtsd2ss xmm0, xmm0
                        emit_byte(buf,0x66);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0);                    // movd eax, xmm0
                    }
                }
            }
            
            emit_byte(buf, 0x50); // push rax
        }

        int arg_base = agg_ret ? 1 : 0;
        int reg_slots = 6 - arg_base;             // integer/pointer registers actually free for real args
        int n = (int)node->call.arg_count;

        // Classify each argument by SysV register CLASS (float vs int/pointer) —
        // floats travel in xmm0..xmm7, a register file entirely separate from the
        // integer/pointer registers, with its OWN independent overflow counter.
        // This was missing entirely before: every argument, float or not, was
        // placed in a GP register, which a real variadic C callee (printf et al.)
        // never inspects for a %f/%lf-style argument — va_arg for floating point
        // reads the xmm save area, not the GP one, so the float's bits were
        // computed correctly but landed somewhere the callee never looked.
        bool is_float[64]; // arg_count is realistically tiny; 64 is a generous ceiling
        int n_float_args = 0, n_int_args = 0;
        for (int i = 0; i < n && i < 64; i++) {
            // Only extern calls (real C functions) need SysV's float-via-xmm
            // routing. Torrent-to-Torrent calls use a separate, internally
            // consistent convention where floats always travel in GP registers
            // (per backend_x64.c's existing float-arithmetic helpers — SSE is
            // used only transiently, the result always comes back to a GP
            // register) — the prologue side for ordinary Torrent functions has
            // never expected to read a float parameter out of an xmm register,
            // so routing floats there for an internal call would silently break
            // every existing float-parameter function, not fix anything.
            is_float[i] = is_extern_call && Type_IsFloat(Type_Infer(node->call.args[i]));
            if (is_float[i]) n_float_args++; else n_int_args++;
        }
        int int_stack = (n_int_args > reg_slots) ? (n_int_args - reg_slots) : 0;
        int float_stack = (n_float_args > 8) ? (n_float_args - 8) : 0;
        // Scoped to the realistic case: NEITHER class overflows its own register
        // budget (6 ints, 8 floats) — true for essentially every real call any
        // caller writes by hand. Simultaneous overflow of both classes at once
        // would need the stack-spill region to interleave two independently-
        // counted overflow groups in original argument order, which the
        // generic-int-only stack-spill logic (see moreargs/* below) does not yet
        // generalize to; that combination is not handled by this fix.
        int n_stack = int_stack + float_stack;

        if (n_stack == 0) {
            // No stack-spilled args at all (the common case). Pop each argument
            // off the top of the (untouched) push stack, in reverse original
            // order — same "pop order matches push order" discipline as before
            // — but route each pop to the xmm or GP register file based on that
            // argument's own type, with two independent counters.
            int gp_idx = arg_base, fp_idx = 0;
            // gp slot assignment per original arg index, computed up front so the
            // reverse-order pop loop below can look up "which register did arg i
            // get" without re-deriving it from a backward scan.
            int gp_slot[64], fp_slot[64];
            for (int i = 0; i < n && i < 64; i++) {
                if (is_float[i]) { fp_slot[i] = fp_idx++; }
                else             { gp_slot[i] = gp_idx++; }
            }
            for (int i = n - 1; i >= 0; i--) {
                if (i >= 64) continue;
                if (is_float[i]) emit_pop_to_xmm(buf, fp_slot[i]);
                else              emit_sysv_arg_reg(buf, gp_slot[i]);
            }
            // AL (for vararg calls) is now set inside emit_call_extern_va, AFTER
            // the callee address is loaded into rax — setting it here as well
            // (the original attempt) doesn't just become redundant, it actively
            // corrupts the address: the call site does movabs rax,<addr> then
            // mov al,<count> here would partially overwrite that address's low
            // byte. Confirmed via a real JIT-buffer disassembly: the double
            // emission produced `mov al,0x2` both before AND after `movabs rax,
            // printf_addr`, leaving rax = printf_addr with its low byte replaced
            // by 0x02 — `call rax` then jumped into unrelated code/data and hit
            // an illegal instruction. Removed here; the indirect-call path keeps
            // its own al-setting (further below) since target-expression
            // evaluation clobbers rax there in a way this fast path never does.
        } else {
            // This branch (today's moreargs/* fix) only generalizes to pure
            // integer/pointer-class stack overflow — it assumes every stack-
            // spilled arg is GP-register-class, AND that consecutive
            // integer-class arguments sit at uniform 8-byte physical stack
            // offsets from each other. Neither holds once a float is mixed in:
            // a float occupies a real push-stack slot regardless of which
            // register class ultimately reads it, so its presence anywhere
            // among the arguments shifts the physical offset of every
            // integer-class argument after it — corrupting BOTH the
            // stack-spill copy (verified via a real crash: a float landing in
            // the spill region got copied as if it were the integer overflow
            // argument) AND the register-loading offsets (verified via a
            // second real crash: with the float moved elsewhere, `rdi` still
            // ended up reading the float's slot instead of the true first
            // argument, because the register-load formula is pure
            // integer-class index arithmetic with no notion that a
            // non-integer-class push sits between two integer-class ones).
            // A full fix needs the offset math to walk ORIGINAL argument
            // order and skip non-matching-class slots, not compute per-class
            // offsets independently — real work, not implemented. Refuse
            // outright instead: any float anywhere in a call whose integer
            // class overflows is unsound with the current offset math,
            // regardless of exactly where that float sits.
            if (float_stack > 0 || (int_stack > 0 && n_float_args > 0)) {
                Error_AtNode(node, "call mixes more than 6 integer/pointer "
                                "arguments with a floating-point argument "
                                "(not supported — stack-spilled arguments of "
                                "mixed float/integer class are not yet "
                                "implemented; reduce the integer/pointer "
                                "argument count to 6 or fewer, or split the "
                                "floating-point arguments into a separate "
                                "call)", NULL);
            }
            int n_reg = n_int_args - int_stack; // == min(n_int_args, reg_slots)
            // In-place reversal of the top n_stack stack slots (args reg_slots..n-1).
            // They currently sit, top-to-bottom, as arg_{n-1} .. arg_{reg_slots}
            // (highest index on top, since it was pushed most recently). Swapping
            // slot k with slot (n_stack-1-k) for k in [0, n_stack/2) flips that to
            // arg_{reg_slots} on top .. arg_{n-1} deepest — the order System V
            // expects relative to the upcoming `call`. Pure stack-slot mov swaps;
            // rax/rcx are free scratch here (nothing else is live in this sequence).
            // rsp does NOT move during this step.
            for (int k = 0; k < n_stack / 2; k++) {
                int off_a = k * 8;
                int off_b = (n_stack - 1 - k) * 8;
                // mov rax, [rsp+off_a]
                emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x84);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off_a);
                // mov rcx, [rsp+off_b]
                emit_byte(buf,0x48);emit_byte(buf,0x8b);emit_byte(buf,0x8c);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off_b);
                // mov [rsp+off_a], rcx
                emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x8c);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off_a);
                // mov [rsp+off_b], rax
                emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0x84);emit_byte(buf,0x24);emit_u32(buf,(uint32_t)off_b);
            }

            // Register-bound args (the lowest n_reg indices) now sit BELOW the
            // reversed stack-arg region, in reverse order, at byte offsets
            // [n_stack*8, (n_stack+n_reg)*8) from the current (unmoved) rsp. Index i
            // (0..n_reg-1) is at offset (n_stack + (n_reg-1-i))*8 — index n_reg-1 is
            // shallowest within that sub-region, index 0 deepest, same "push order
            // puts the highest index on top" rule as everywhere else, just shifted
            // down by the n_stack region's size. Read by mov (NOT pop — popping
            // would consume the stack-arg region above, not this one); rsp is left
            // untouched until every register has been loaded.
            // Floats are GUARANTEED to fit in xmm0..xmm7 in this branch (the guard
            // above already rejected float_stack > 0), regardless of whether the
            // INTEGER class overflows. They never participated in the reversal
            // above (that's purely the integer-class overflow region) and don't
            // sit in the "n_reg register-bound ints" sub-region either — they're
            // wherever their own original push position put them, found by
            // walking original argument order and computing each float's distance
            // from the current (still-unmoved) rsp.
            {
                int fp_idx = 0;
                for (int i = 0; i < n && i < 64; i++) {
                    if (!is_float[i]) continue;
                    int off = (n - 1 - i) * 8; // original push-order offset from rsp
                    emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x84);
                    emit_byte(buf, 0x24); emit_u32(buf, (uint32_t)off); // mov rax, [rsp+off]
                    switch (fp_idx) {
                        case 0: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0); break;
                        case 1: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc8); break;
                        case 2: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xd0); break;
                        case 3: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xd8); break;
                        case 4: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xe0); break;
                        case 5: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xe8); break;
                        case 6: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xf0); break;
                        case 7: emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xf8); break;
                    }
                    fp_idx++;
                }
            }
            for (int i = 0; i < n_reg; i++) {
                int off = (n_stack + (n_reg - 1 - i)) * 8;
                emit_mov_argreg_from_rsp_off(buf, arg_base + i, off);
            }
            if (is_vararg) {
                emit_byte(buf, 0xb0); emit_byte(buf, (uint8_t)n_float_args); // mov al, n_float_args
            }
            // rsp is already correct for `call`: it still points at offset 0, the
            // top of the (already correctly-ordered) stack-arg region, exactly as
            // it did right after the reversal step — reading the register region
            // via mov never moved rsp. The register region's now-dead bytes sit
            // BELOW the stack-arg region (deeper in memory) and need no explicit
            // cleanup here; they're reclaimed wholesale by `mov rsp, r14` once the
            // call returns, same as every other call-scratch byte.
        }
        if (agg_ret) {
            emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xdf); // mov rdi, rbx (hidden result ptr -> arg 0)
        }

        // call <rel32> for direct, call rax for indirect
        if (node->call.target_name) {
            // For a generic instantiation call, resolve (or create+enqueue) the
            // concrete instantiation and target ITS symbol; otherwise target the
            // ordinary function symbol. Fixups patch to sym->offset at finalize.
            Symbol* call_target = node->call.sym;
            if (node->call.type_arg_count > 0 && node->call.sym && node->call.sym->generic_decl) {
                call_target = Generic_Instantiate(node->call.sym,
                                                node->call.type_args,
                                                node->call.type_arg_count);
            }
            if (call_target->is_extern) {
                void* addr = Extern_Resolve(call_target->name);
                emit_call_extern_va(buf, addr, call_target->name, is_vararg ? n_float_args : -1, n_stack * 8, false);
            } else {
                emit_byte(buf, 0xe8);
                add_fixup(buf->size, call_target);
                emit_u32(buf, 0);
            }
        } else if (direct_lit_sym) {
            // `cmp(a,b)` where `cmp`'s type resolved to TYPE_FN_LITERAL: the
            // callee is known at compile time (it's the specific function this
            // generic instantiation was bound to), so this compiles to exactly
            // the same instruction a direct `asc(a,b)` call would -- no runtime
            // load of a function-pointer value, no indirect `call rax`. This is
            // the actual payoff of TYPE_FN_LITERAL over a plain TYPE_FUNCTION
            // parameter: the parameter still EXISTS as an ordinary local (so
            // `cmp` can be passed on, stored, compared, etc. like any value),
            // but the call site that already knows which function it is doesn't
            // pay an indirection for that.
            if (direct_lit_sym->is_extern) {
                void* addr = Extern_Resolve(direct_lit_sym->name);
                emit_call_extern_va(buf, addr, direct_lit_sym->name, is_vararg ? n_float_args : -1, n_stack * 8, false);
            } else {
                emit_byte(buf, 0xe8);
                add_fixup(buf->size, direct_lit_sym);
                emit_u32(buf, 0);
            }
        } else {
            compile_node_ctx(buf, node->call.target_expr, loop);
            // Reuse the same, single, tested SysV calling-convention
            // implementation direct extern calls use — one real
            // implementation parameterized on where the callee address
            // comes from (addr_in_rax=true here, since it's a runtime
            // value already in rax rather than a compile-time-known
            // pointer), not a second hand-rolled copy of the same
            // non-trivial alignment/stack-arg-copy machinery. Applied
            // unconditionally for every indirect call, any argument count:
            // costs nothing extra for a Torrent-to-Torrent callee (which
            // doesn't care about SysV alignment) and is simply correct if
            // the callee turns out to be extern, which can't be ruled out
            // at compile time for an indirect call (no symbol to check
            // is_extern against — see the is_extern_call comment above).
            emit_call_extern_va(buf, NULL, NULL, is_vararg ? n_float_args : -1, n_stack * 8, true);
        }

        if (agg_ret) {
            // For sret the result is the pointer we passed (still in rbx). But that
            // points into the scratch we're about to reclaim — so copy the bytes into
            // a stable spot is unnecessary here because the *consumer* (decl/assign)
            // copies immediately. We hand back the pointer in rax; restore rsp AFTER
            // capturing, but the consumer reads through rax before any further push.
            emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xd8); // mov rax, rbx
        }
        // Reclaim all call scratch.
        emit_byte(buf, 0x4c); emit_byte(buf, 0x89); emit_byte(buf, 0xf4); // mov rsp, r14
        emit_byte(buf, 0x5b);                                            // pop rbx
        emit_byte(buf, 0x41); emit_byte(buf, 0x5e);                       // pop r14
        return;
    }
    
    if (node->type == AST_IF) {
        compile_node_ctx(buf, node->if_stmt.condition, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0); // test rax, rax
        emit_byte(buf, 0x0f); emit_byte(buf, 0x84);
        size_t jmp_false_offset = buf->size; emit_u32(buf, 0);
        
        compile_node_ctx(buf, node->if_stmt.true_block, loop);
        
        if (node->if_stmt.false_block) {
            emit_byte(buf, 0xe9);
            size_t jmp_end_offset = buf->size; emit_u32(buf, 0);
            *(uint32_t*)(buf->code + jmp_false_offset) = buf->size - (jmp_false_offset + 4);
            compile_node_ctx(buf, node->if_stmt.false_block, loop);
            *(uint32_t*)(buf->code + jmp_end_offset) = buf->size - (jmp_end_offset + 4);
        } else {
            *(uint32_t*)(buf->code + jmp_false_offset) = buf->size - (jmp_false_offset + 4);
        }
        return;
    }
    
    if (node->type == AST_WHILE) {
        LoopContext new_loop = {0};
        new_loop.start_offset = buf->size;
        new_loop.break_offsets = (size_t*)malloc(4 * sizeof(size_t));
        new_loop.break_capacity = 4;
        new_loop.continue_offsets = (size_t*)malloc(4 * sizeof(size_t));
        new_loop.continue_capacity = 4;
        new_loop.parent = loop;
        new_loop.defer_base = s_defer_stack;

        compile_node_ctx(buf, node->while_stmt.condition, &new_loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x84);
        size_t jmp_end_offset = buf->size; emit_u32(buf, 0);

        compile_node_ctx(buf, node->while_stmt.body, &new_loop);
        // `continue` in a while jumps back to the condition (= start).
        for (size_t i = 0; i < new_loop.continue_count; i++) {
            size_t off = new_loop.continue_offsets[i];
            *(uint32_t*)(buf->code + off) = new_loop.start_offset - (off + 4);
        }
        emit_byte(buf, 0xe9);
        emit_u32(buf, (uint32_t)(new_loop.start_offset - (buf->size + 4)));

        *(uint32_t*)(buf->code + jmp_end_offset) = buf->size - (jmp_end_offset + 4);
        for (size_t i = 0; i < new_loop.break_count; i++) {
            size_t off = new_loop.break_offsets[i];
            *(uint32_t*)(buf->code + off) = buf->size - (off + 4);
        }
        free(new_loop.break_offsets);
        free(new_loop.continue_offsets);
        return;
    }

    if (node->type == AST_FOR) {
        // init ; LOOP: cond? jz END ; body ; CONT: incr ; jmp LOOP ; END:
        // `continue` lands on CONT (the increment), `break` on END.
        compile_node_ctx(buf, node->for_stmt.init, loop);

        LoopContext nl = {0};
        nl.break_offsets = (size_t*)malloc(4 * sizeof(size_t));
        nl.break_capacity = 4;
        nl.continue_offsets = (size_t*)malloc(4 * sizeof(size_t));
        nl.continue_capacity = 4;
        nl.parent = loop;
        nl.defer_base = s_defer_stack;
        nl.start_offset = buf->size; // top = condition

        compile_node_ctx(buf, node->for_stmt.cond, &nl);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0); // test rax, rax
        emit_byte(buf, 0x0f); emit_byte(buf, 0x84);
        size_t jmp_end = buf->size; emit_u32(buf, 0);

        compile_node_ctx(buf, node->for_stmt.body, &nl);

        // continue target = the increment.
        size_t cont_target = buf->size;
        for (size_t i = 0; i < nl.continue_count; i++) {
            size_t off = nl.continue_offsets[i];
            *(uint32_t*)(buf->code + off) = cont_target - (off + 4);
        }
        compile_node_ctx(buf, node->for_stmt.incr, &nl);
        // jmp back to condition
        emit_byte(buf, 0xe9);
        emit_u32(buf, (uint32_t)(nl.start_offset - (buf->size + 4)));

        *(uint32_t*)(buf->code + jmp_end) = buf->size - (jmp_end + 4);
        for (size_t i = 0; i < nl.break_count; i++) {
            size_t off = nl.break_offsets[i];
            *(uint32_t*)(buf->code + off) = buf->size - (off + 4);
        }
        free(nl.break_offsets);
        free(nl.continue_offsets);
        return;
    }

    if (node->type == AST_BREAK) {
        if (!loop) exit(1);
        emit_defers(buf, loop->defer_base, loop);
        if (loop->break_count >= loop->break_capacity) {
            loop->break_capacity *= 2;
            loop->break_offsets = (size_t*)realloc(loop->break_offsets, loop->break_capacity * sizeof(size_t));
        }
        emit_byte(buf, 0xe9);
        loop->break_offsets[loop->break_count++] = buf->size;
        emit_u32(buf, 0);
        return;
    }

    if (node->type == AST_CONTINUE) {
        if (!loop) exit(1);
        emit_defers(buf, loop->defer_base, loop);
        if (loop->continue_count >= loop->continue_capacity) {
            loop->continue_capacity *= 2;
            loop->continue_offsets = (size_t*)realloc(loop->continue_offsets, loop->continue_capacity * sizeof(size_t));
        }
        emit_byte(buf, 0xe9);
        loop->continue_offsets[loop->continue_count++] = buf->size;
        emit_u32(buf, 0);
        return;
    }
    
    if (node->type == AST_DECLARATION) {
        Symbol* sym = node->decl.sym;
        if (!sym) exit(1);
        Type* vt = sym->type;

        // Recalculate local variable offset on the fly (needed for generic instantiations).
        // Only for true locals: a global declaration keeps the offset assigned at
        // parse time (into the static image). Recomputing it here as a frame slot
        // would clobber the global's image offset — and worse, give every global the
        // same stale offset, aliasing them onto one another.
        if (s_func && sym->kind == SYM_LOCAL) {
            uint64_t sz = Type_SizeOf(vt);
            uint64_t slot = (sz + 7) & ~7ULL;
            s_func->current_local_offset += slot;
            sym->offset = s_func->current_local_offset;
        }

        // Struct-typed declaration.
        if (vt && vt->cls == TYPE_STRUCT) {
            // Zero the storage first (unspecified fields zero-init, C99).
            uint64_t sz = Type_SizeOf(vt);
            emit_var_addr(buf, sym);                                  // rax = &var
            emit_byte(buf, 0x49); emit_byte(buf, 0x89); emit_byte(buf, 0xc0); // mov r8, rax (save base)
            for (uint64_t b = 0; b < sz; b += 8) {
                // mov qword [r8 + b], 0
                emit_byte(buf, 0x49); emit_byte(buf, 0xc7); emit_byte(buf, 0x80);
                emit_u32(buf, (uint32_t)b);
                emit_u32(buf, 0);
            }
            if (node->decl.init_expr && node->decl.init_expr->type == AST_STRUCT_LITERAL) {
                // Fill via the shared materializer (handles nested aggregate fields).
                emit_var_addr(buf, sym);
                emit_byte(buf, 0x50); // push dest base
                emit_fill_literal(buf, node->decl.init_expr, loop);
                emit_byte(buf, 0x58); // pop dest base
            } else if (node->decl.init_expr) {
                // Struct copy: init yields the source struct's address. The init may be
                // a call (clobbers r8), so recompute the dest after evaluating source.
                Type* it = Type_Infer(node->decl.init_expr);
                if (!it || it->cls != TYPE_STRUCT || !Type_Equals(it, vt)) {
                    Error_AtNode(node, "compiler bug: struct initializer type mismatch reached backend", NULL);
                }
                compile_node_ctx(buf, node->decl.init_expr, loop); // rax = &source
                emit_byte(buf, 0x50);                              // push src addr
                emit_var_addr(buf, sym);                           // rax = &dest (fresh)
                emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc7); // mov rdi, rax (dst)
                emit_byte(buf, 0x5e);                              // pop rsi (src)
                emit_memcpy_rdi_rsi(buf, sz);
            }
            return;
        }

        // Array-typed declaration.
        if (vt && vt->cls == TYPE_ARRAY) {
            uint64_t sz = Type_SizeOf(vt);
            // Zero storage first (unspecified elements zero-init).
            emit_var_addr(buf, sym);
            emit_byte(buf, 0x49); emit_byte(buf, 0x89); emit_byte(buf, 0xc0); // mov r8, rax
            for (uint64_t b = 0; b < sz; b += 8) {
                emit_byte(buf, 0x49); emit_byte(buf, 0xc7); emit_byte(buf, 0x80);
                emit_u32(buf, (uint32_t)b); emit_u32(buf, 0);
            }
            if (node->decl.init_expr && node->decl.init_expr->type == AST_ARRAY_LITERAL) {
                emit_var_addr(buf, sym);
                emit_byte(buf, 0x50); // push dest base
                emit_fill_literal(buf, node->decl.init_expr, loop);
                emit_byte(buf, 0x58); // pop dest base
            } else if (node->decl.init_expr) {
                // Array copy from another array value (address in rax).
                Type* it = Type_Infer(node->decl.init_expr);
                if (!it || it->cls != TYPE_ARRAY || !Type_Equals(it, vt)) {
                    Error_AtNode(node, "compiler bug: array initializer type mismatch reached backend", NULL);
                }
                compile_node_ctx(buf, node->decl.init_expr, loop);
                emit_byte(buf, 0x50);
                emit_var_addr(buf, sym);
                emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc7); // mov rdi, rax
                emit_byte(buf, 0x5e);                                            // pop rsi
                emit_memcpy_rdi_rsi(buf, sz);
            }
            return;
        }

        // Scalar declaration (8-byte slot, width handled on read/store).
        // A global scalar's value is folded at compile time and written into the
        // static image before main, so emit NO runtime code for it here (emitting
        // a store would clobber the pre-written initial value).
        if (sym->kind == SYM_GLOBAL) {
            return;
        }
        if (node->decl.init_expr) {
            compile_node_ctx(buf, node->decl.init_expr, loop);
        } else {
            emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xc0);
        }
        emit_byte(buf, 0x50); // push rax (value)
        emit_var_addr(buf, sym); // rax = dest address
        emit_byte(buf, 0x59); // pop rcx (value)
        
        Type* it = node->decl.init_expr ? Type_Infer(node->decl.init_expr) : NULL;
        emit_coerce_rcx_int_to_float(buf, sym->type, it);
        if (!emit_store_scalar_float(buf, sym->type)) {
            emit_store_to_rax(buf, Type_Width(sym->type));
        }
        return;
    }
    
    if (node->type == AST_IDENT) {
        Symbol* sym = node->ident.sym;
        if (!sym) { printf("NULL SYM FOR IDENTIFIER: %.*s\n", (int)node->ident.name_len, node->ident.name); exit(1); }

        // Aggregate-typed ident: its "value" is its address (no load).
        if (Type_IsAggregate(sym->type)) {
            emit_var_addr(buf, sym);
            return;
        }

        if (sym->kind == SYM_GLOBAL || sym->kind == SYM_LOCAL) {
            emit_var_addr(buf, sym);
            if (!emit_load_scalar_float(buf, sym->type)) {
                emit_load_from_rax(buf, Type_Width(sym->type), Type_IsSigned(sym->type));
            }
        } else if (sym->kind == SYM_FUNCTION) {
            // Generic function in value position: monomorphize and fixup to the instance.
            Symbol* target_sym = sym;
            if (node->ident.type_arg_count > 0 && sym->generic_decl) {
                target_sym = Generic_Instantiate(sym, node->ident.type_args, node->ident.type_arg_count);
            }
            if (!target_sym->is_extern) {
                printf("Function %.*s resolved: offset=%d, JIT=%p, addr=%p\n", (int)target_sym->name_len, target_sym->name, target_sym->offset, buf->code, buf->code + target_sym->offset);
            }
            emit_fn_symbol_value(buf, target_sym);
        }
        return;
    }
    
    if (node->type == AST_ASSIGN) {
        Type* lt = Type_Infer(node->binary.left);

        // Whole-aggregate assignment: copy all bytes (value semantics).
        if (Type_IsAggregate(lt)) {
            Type* rt = Type_Infer(node->binary.right);
            if (!rt || !Type_Equals(lt, rt)) {
                Error_AtNode(node, "compiler bug: aggregate assignment type mismatch reached backend", NULL);
            }
            compile_node_ctx(buf, node->binary.right, loop); // rax = &source
            emit_byte(buf, 0x50); // push src addr
            compile_lvalue(buf, node->binary.left, loop);    // rax = dest addr
            emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc7); // mov rdi, rax (dst)
            emit_byte(buf, 0x5e); // pop rsi (src)
            emit_memcpy_rdi_rsi(buf, Type_SizeOf(lt));
            return;
        }

        if (node->binary.is_compound) {
            // `a op= b`, desugared to `a = a <op> b` with `a` shared between the
            // assign target (node->binary.left) and the binop's own left operand
            // (node->binary.right->binary.left, same pointer). Compute a's address
            // ONCE here, spill it, and memo it under that shared node pointer so
            // the binop's later walk of the same node (inside compile_node_ctx on
            // node->binary.right, below) loads the spill instead of recomputing
            // the address -- which for a complex lvalue like arr[idx()] would both
            // call idx() a second time and land on a different element entirely.
            ASTNode* shared_lvalue = node->binary.left;

            uint64_t slot = 8;
            s_func->current_local_offset += (int)slot;
            int off = s_func->current_local_offset; // address spilled at [rbp-off]

            compile_lvalue(buf, shared_lvalue, loop); // rax = dest address (first & only real computation)
            emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0x85); emit_u32(buf, (uint32_t)(-off)); // mov [rbp-off], rax

            // Save/restore around the binop so nested compound assigns (the binop's
            // own right side could itself contain another `x op= y`) don't clobber
            // this memo entry before we're done with it.
            int save_depth = s_ca_depth;
            if (s_ca_depth < CA_MEMO_MAX) {
                s_ca_node[s_ca_depth] = shared_lvalue;
                s_ca_slot[s_ca_depth] = off;
                s_ca_depth++;
            }

            compile_node_ctx(buf, node->binary.right, loop); // evaluates the binop: reads `a` (memo hit) op b -> rax

            s_ca_depth = save_depth;

            emit_byte(buf, 0x50); // push rax (computed value)
            emit_byte(buf, 0x48); emit_byte(buf, 0x8b); emit_byte(buf, 0x85); emit_u32(buf, (uint32_t)(-off)); // mov rax, [rbp-off] (dest addr, spilled once above)
            emit_byte(buf, 0x59); // pop rcx (value)
            // Same store sequence as the plain-assign path below: address in rax, value in rcx.
            emit_coerce_rcx_int_to_float(buf, lt, Type_Infer(node->binary.right));
            if (!emit_store_scalar_float(buf, lt)) {
                emit_store_to_rax(buf, Type_Width(lt));
            }
            emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc8); // mov rax, rcx (result = stored value)
            return;
        }

        compile_node_ctx(buf, node->binary.right, loop);
        emit_byte(buf, 0x50); // push rax (value)
        compile_lvalue(buf, node->binary.left, loop);  // rax = dest address
        emit_byte(buf, 0x59); // pop rcx (value)
        // int -> float coercion at the store boundary (f64 x = 1 stores 1.0).
        emit_coerce_rcx_int_to_float(buf, lt, Type_Infer(node->binary.right));
        // Float destinations round to their precision (f32 narrows); others store by width.
        if (!emit_store_scalar_float(buf, lt)) {
            emit_store_to_rax(buf, Type_Width(lt));
        }
        // result of assignment is the stored value; leave it in rax
        emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc8); // mov rax, rcx
        return;
    }
    
    if (node->type == AST_ADDR) {
        compile_lvalue(buf, node->unary, loop);
        return;
    }

    if (node->type == AST_CAST) {
        compile_node_ctx(buf, node->cast.expr, loop); // value in rax (int bits, or float bit pattern)
        Type* target = node->cast.target_type;
        Type* src = Type_Infer(node->cast.expr);
        
        if (src && src->cls == TYPE_STRUCT && target && target->cls == TYPE_STRUCT) {
            if (node->cast.struct_downcast_offset > 0) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x05); 
                emit_u32(buf, (uint32_t)node->cast.struct_downcast_offset);
            }
            return;
        }
        
        bool src_f = Type_IsFloat(src);
        bool tgt_f = Type_IsFloat(target);

        if (src_f || tgt_f) {
            if (src_f && tgt_f) {
                // float -> float: f64 and f32 share the same in-register double bits
                // in this model (we compute in double), so the cast is a no-op at the
                // bit level. (A true f32 storage representation would convert here.)
            } else if (!src_f && tgt_f) {
                // int -> float: cvtsi2sd xmm0, rax  (F2 48 0F 2A C0) ; movq rax, xmm0
                emit_byte(buf,0xf2);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x2a);emit_byte(buf,0xc0);
                emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0);
            } else {
                // float -> int: movq xmm0, rax ; cvttsd2si rax, xmm0  (F2 48 0F 2C C0) [truncating]
                emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0);
                emit_byte(buf,0xf2);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x2c);emit_byte(buf,0xc0);
                // then narrow to the integer target width, respecting sign —
                // same helper the binary-op tails use; target is confirmed
                // non-float here (this is the src_f-and-!tgt_f branch), so
                // the helper's float early-returns are simply unreached.
                emit_narrow_rax_to_type(buf, target);
            }
            return;
        }

        // Truncate/extend the in-register value to the target width — same
        // helper as above; kept as one shared function rather than a third
        // copy of the same width/sign switch in this file (the binary-op
        // tails and the float-to-int path just above are the other two).
        emit_narrow_rax_to_type(buf, target);
        return;
    }

    if (node->type == AST_DEREF) {
        // Route through compile_lvalue instead of calling compile_node_ctx(node->unary,
        // ...) directly: for a deref, the address IS the pointer value, so the two are
        // semantically identical -- but going through compile_lvalue(node, ...) means
        // this node is checked against the compound-assign memo first. Without this, a
        // compound assign through a complex pointer expression (`*ptr() += 5`) bypassed
        // the memo on its rvalue read and called ptr() a second time, same bug class as
        // arr[idx()] += 5.
        compile_lvalue(buf, node, loop); // pointer value in rax (address-of-pointee == pointer itself)
        Type* pt = Type_Infer(node); // the pointee type (result of the deref)
        if (Type_IsAggregate(pt)) {
            return; // aggregate pointee: value is its address (leave rax)
        }
        // Scalar pointee: width-correct load (a u8* deref must read 1 byte, not 8).
        if (!emit_load_scalar_float(buf, pt)) {
            emit_load_from_rax(buf, Type_Width(pt), Type_IsSigned(pt));
        }
        return;
    }

    if (node->type == AST_FIELD) {
        Type_Infer(node); // ensure field.field/sdef are resolved
        compile_lvalue(buf, node, loop); // field address in rax
        Type* ft = node->field.field ? node->field.field->type : NULL;
        if (Type_IsAggregate(ft)) {
            // Aggregate field: value is its address (leave rax as-is).
            return;
        }
        if (!emit_load_scalar_float(buf, ft)) {
            emit_load_from_rax(buf, Type_Width(ft), Type_IsSigned(ft));
        }
        return;
    }

    if (node->type == AST_INDEX) {
        compile_lvalue(buf, node, loop); // element address in rax
        Type* et = Type_Infer(node);     // element type
        if (Type_IsAggregate(et)) {
            return; // aggregate element: value is its address
        }
        if (!emit_load_scalar_float(buf, et)) {
            emit_load_from_rax(buf, Type_Width(et), Type_IsSigned(et));
        }
        return;
    }

    if (node->type == AST_STRUCT_DECL) {
        return; // definition emits no code
    }

    if (node->type == AST_INT_LITERAL && node->lit_kind == LIT_FLOAT) {
        // Float value travels in rax as its raw IEEE-754 bit pattern, just like
        // every other value uses rax as the accumulator. SSE registers are used
        // only transiently inside float arithmetic/compare, then the result bits
        // come back to rax. f64 literal here; f32 narrowing happens on store/cast.
        double d = node->float_value;
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, bits);
        return;
    }

    if (node->type == AST_INT_LITERAL && node->lit_kind == LIT_FN_SYMBOL) {
        // int_value is a Symbol* (a SYM_FUNCTION), not a real integer — see
        // clone_ast's const-generic fn-param substitution (types.c) and the
        // LIT_FN_SYMBOL comment in compiler.h. Any AST shape that calls this
        // literal DIRECTLY is already handled more cheaply by the
        // direct-call shortcut in AST_CALL above (a real `call rel32`
        // instead of loading a pointer first); this is the fallback for
        // every other use — assigned to a variable, returned, stored in a
        // struct field, passed onward — which must resolve to the
        // function's actual address, not the raw bookkeeping pointer.
        Symbol* sym = (Symbol*)(intptr_t)node->int_value;
        if (!sym || sym->kind != SYM_FUNCTION) {
            Error_AtNode(node, "compiler bug: LIT_FN_SYMBOL literal doesn't hold a valid function symbol", NULL);
        }
        emit_fn_symbol_value(buf, sym);
        return;
    }

    if (node->type == AST_INT_LITERAL) {
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, node->int_value);
        return;
    }
    if (node->type == AST_SIZEOF) {
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, Type_SizeOf(node->sizeof_expr.type));
        return;
    }
    if (node->type == AST_ALIGNOF) {
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, Type_AlignOf(node->sizeof_expr.type));
        return;
    }
    if (node->type == AST_OFFSETOF) {
        // Real fallback case, mirroring AST_SIZEOF just above: reached only
        // when ConstEval couldn't fold this earlier (e.g. it survived inside
        // a non-generic-const context). Both the type and the field index
        // must be fully concrete by codegen time -- there is no further
        // deferral point past this.
        Type* t = node->field_ref_expr.type;
        int64_t idx;
        if (!t || t->cls != TYPE_STRUCT || !ConstEval(node->field_ref_expr.index_expr, &idx)) {
            Error_AtNode(node, "offsetof: could not resolve struct type or field index", NULL);
        }
        StructDef* sd = Struct_Find(t->struct_name);
        if (!sd) { Error_AtNode(node, "offsetof: unknown struct type", NULL); }
        Struct_Layout(sd);
        if (idx < 0 || (uint64_t)idx >= sd->field_count) {
            Error_AtNode(node, "offsetof: field index out of range", NULL);
        }
        emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, sd->fields[idx].offset);
        return;
    }
    if (node->type == AST_NAMEOF) {
        // Should never happen: nameof always folds into a real AST_STRING
        // either at parse time or in clone_ast at generic instantiation
        // (see types.c). Reaching codegen still as AST_NAMEOF means some
        // nesting level didn't get concretized -- fail loud, not silent.
        Error_AtNode(node, "nameof: struct type or field index never resolved to a constant", NULL);
    }

    if (node->type == AST_STRING) {
        if (g_aot_mode) {
            const char* str_ptr = (const char*)(uintptr_t)node->int_value;
            size_t rodata_off = ELF_AddString(str_ptr, strlen(str_ptr));
            // lea rax, [rip + offset]
            emit_byte(buf, 0x48); emit_byte(buf, 0x8d); emit_byte(buf, 0x05);
            ELF_AddRelocation(buf->size, ELF_RELOC_STRING, NULL, rodata_off);
            emit_u32(buf, 0); // patched by linker
        } else {
            // AST_STRING's int_value is the address of its static bytes (movabs rax, ptr).
            emit_byte(buf, 0x48); emit_byte(buf, 0xb8); emit_u64(buf, node->int_value);
        }
        return;
    }
    
    if (node->type == AST_LOGICAL_NOT) {
        compile_node_ctx(buf, node->unary, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x94); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb6); emit_byte(buf, 0xc0);
        return;
    }

    if (node->type == AST_BIT_NOT) {
        compile_node_ctx(buf, node->unary, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0xf7); emit_byte(buf, 0xd0); // not rax
        return;
    }

    if (node->type == AST_LOGICAL_AND) {
        compile_node_ctx(buf, node->binary.left, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x84);
        size_t jmp_offset = buf->size; emit_u32(buf, 0);
        
        compile_node_ctx(buf, node->binary.right, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x95); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb6); emit_byte(buf, 0xc0);
        
        *(uint32_t*)(buf->code + jmp_offset) = buf->size - (jmp_offset + 4);
        return;
    }

    if (node->type == AST_LOGICAL_OR) {
        compile_node_ctx(buf, node->binary.left, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x85);
        size_t jmp_true_offset = buf->size; emit_u32(buf, 0);
        
        compile_node_ctx(buf, node->binary.right, loop);
        emit_byte(buf, 0x48); emit_byte(buf, 0x85); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x0f); emit_byte(buf, 0x95); emit_byte(buf, 0xc0);
        emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb6); emit_byte(buf, 0xc0);
        
        emit_byte(buf, 0xe9);
        size_t jmp_end_offset = buf->size; emit_u32(buf, 0);
        
        *(uint32_t*)(buf->code + jmp_true_offset) = buf->size - (jmp_true_offset + 4);
        emit_byte(buf, 0x48); emit_byte(buf, 0xc7); emit_byte(buf, 0xc0); emit_u32(buf, 1); // is_true: mov rax, 1
        *(uint32_t*)(buf->code + jmp_end_offset) = buf->size - (jmp_end_offset + 4);
        return;
    }

    compile_node_ctx(buf, node->binary.left, loop);
    emit_byte(buf, 0x50); // push rax
    compile_node_ctx(buf, node->binary.right, loop);
    emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xc1); // mov rcx, rax
    emit_byte(buf, 0x58); // pop rax

    Type* lt = Type_Infer(node->binary.left);
    Type* rt = Type_Infer(node->binary.right);

    // --- Wordwise aggregate binops ------------------------------------------
    // Aggregate operands arrive as ADDRESSES (rax=&L, rcx=&R). Lowered lanewise
    // via the shared traversal (codegen.c: agg_binop_apply/Agg_Lanes) so the
    // legal-op set (==,!=,+,-,*,&,|,^) and lane enumeration can't drift from
    // constexpr's copy the way the old duplicated version could. Only this
    // sink's do_lane/finish_eq differ: constexpr reads two arena buffers,
    // this one emits x86 that computes the values at runtime.
    if (Type_IsAggregate(lt) || Type_IsAggregate(rt)) {
        if (!lt || !rt || !Type_Equals(lt, rt)) {
            Error_AtNode(node, "compiler bug: binary operator on mismatched operand types reached backend", NULL);
        }
        X64AggBinopCtx actx = { .buf = buf, .func = s_func, .is_eq_mode = (node->type == AST_EQ || node->type == AST_NEQ) };
        if (!actx.is_eq_mode) {
            uint64_t sz = Type_SizeOf(lt);
            uint64_t slot = (sz + 15) & ~(uint64_t)15;
            s_func->current_local_offset += (int)slot;
            actx.dst_off = s_func->current_local_offset;
            emit_byte(buf,0x48);emit_byte(buf,0x8d);emit_byte(buf,0x95);emit_u32(buf,(uint32_t)(-actx.dst_off)); // lea rdx,[rbp-off]
        } else {
            emit_byte(buf,0x48);emit_byte(buf,0x31);emit_byte(buf,0xd2); // xor rdx, rdx (running diff accumulator)
        }
        AggBinopSink sink = { .ctx = &actx, .do_lane = x64_agg_do_lane, .finish_eq = x64_agg_finish_eq };
        if (!agg_binop_apply(node->type, lt, rt, &sink)) {
            Error_AtNode(node, "compiler bug: aggregate operator not defined reached backend", NULL);
        }
        if (actx.is_eq_mode) {
            emit_byte(buf,0x48);emit_byte(buf,0x85);emit_byte(buf,0xd2);              // test rdx, rdx
            emit_byte(buf,0x0f);emit_byte(buf,node->type==AST_EQ?0x94:0x95);emit_byte(buf,0xc2); // sete/setne dl
            emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xd2);     // movzx rdx->...
            emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xd0);              // mov rax, rdx
        } else {
            emit_byte(buf,0x48);emit_byte(buf,0x89);emit_byte(buf,0xd0);              // mov rax, rdx (result = temp addr)
        }
        return;
    }

    // --- Floating-point path -------------------------------------------------
    // If either operand is float, the operation is float (operands already
    // promoted by arith_result). Bits are in rax (left) and rcx (right); move
    // them into xmm0/xmm1 with movq, do the SSE op, move the result back to rax.
    // We compute everything in f64 (double); an f32 result is narrowed by the
    // surrounding store/cast. This keeps the rax-accumulator model intact.
    if (Type_IsFloat(lt) || Type_IsFloat(rt)) {
        // Mixed int/float: convert the INTEGER operand to f64 before the SSE op.
        // arith_result picks a float RESULT type but emits no operand conversion, so
        // `2.5 + 1` must turn the int `1` (rcx) into 1.0 here, else its raw bits would
        // be reinterpreted as a double. Left operand in rax, right in rcx.
        if (!Type_IsFloat(lt)) {
            // cvtsi2sd xmm0, rax ; movq rax, xmm0
            emit_byte(buf,0xf2);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x2a);emit_byte(buf,0xc0);
            emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0);
        }
        if (!Type_IsFloat(rt)) {
            // cvtsi2sd xmm1, rcx ; movq rcx, xmm1
            emit_byte(buf,0xf2);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x2a);emit_byte(buf,0xc9);
            emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc9);
        }
        // movq xmm0, rax  (66 48 0F 6E C0) ; movq xmm1, rcx (66 48 0F 6E C9)
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc0);
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x6e);emit_byte(buf,0xc9);
        switch (node->type) {
            case AST_ADD: // addsd xmm0, xmm1  (F2 0F 58 C1)
                emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x58);emit_byte(buf,0xc1); break;
            case AST_SUB: // subsd  (F2 0F 5C C1)
                emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x5c);emit_byte(buf,0xc1); break;
            case AST_MUL: // mulsd  (F2 0F 59 C1)
                emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x59);emit_byte(buf,0xc1); break;
            case AST_DIV: // divsd  (F2 0F 5E C1)
                emit_byte(buf,0xf2);emit_byte(buf,0x0f);emit_byte(buf,0x5e);emit_byte(buf,0xc1); break;
            case AST_EQ: case AST_NEQ: case AST_LT:
            case AST_GT: case AST_LTE: case AST_GTE: {
                // ucomisd xmm0, xmm1 (66 0F 2E C1), then set* on the resulting flags.
                // ucomisd sets CF/ZF like an unsigned compare: below->CF, equal->ZF.
                emit_byte(buf,0x66);emit_byte(buf,0x0f);emit_byte(buf,0x2e);emit_byte(buf,0xc1);
                emit_byte(buf,0x0f);
                switch (node->type) {
                    case AST_EQ:  emit_byte(buf,0x94); break; // sete
                    case AST_NEQ: emit_byte(buf,0x95); break; // setne
                    case AST_LT:  emit_byte(buf,0x92); break; // setb   (a<b)
                    case AST_GT:  emit_byte(buf,0x97); break; // seta   (a>b)
                    case AST_LTE: emit_byte(buf,0x96); break; // setbe  (a<=b)
                    case AST_GTE: emit_byte(buf,0x93); break; // setae  (a>=b)
                    default: break;
                }
                emit_byte(buf,0xc0); // ...al
                emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0xb6);emit_byte(buf,0xc0); // movzx rax, al
                return; // result is an integer bool already in rax
            }
            default:
                Error_AtNode(node, "compiler bug: invalid floating-point op reached backend", NULL);
        }
        // movq rax, xmm0  (66 48 0F 7E C0)
        emit_byte(buf,0x66);emit_byte(buf,0x48);emit_byte(buf,0x0f);emit_byte(buf,0x7e);emit_byte(buf,0xc0);
        // Everything above computes in f64 regardless of the logical result
        // type — narrow to f32 here if that's what Type_Infer(node) says,
        // same reasoning and same helper as the integer tail below (this
        // path returns early, so it needs its own call rather than falling
        // through to that one).
        emit_narrow_rax_to_type(buf, Type_Infer(node));
        return;
    }

    // Signedness of the operation comes from the operands (arith-converted),
    // not the result (e.g. `u32 < u32` is bool-typed but an unsigned compare).
    bool op_signed;
    if (node->type == AST_SHL || node->type == AST_SHR) {
        op_signed = Type_IsSigned(lt); // shift signedness follows the value being shifted
    } else {
        // wider operand wins; tie -> unsigned (matches Type_Infer's arith rule)
        int wl = Type_Width(lt), wr = Type_Width(rt);
        if (wl > wr)      op_signed = Type_IsSigned(lt);
        else if (wr > wl) op_signed = Type_IsSigned(rt);
        else              op_signed = Type_IsSigned(lt) && Type_IsSigned(rt);
    }

    // --- Pointer arithmetic: scale by pointee size --------------------------
    // `ptr + i` / `ptr - i` must advance by i * sizeof(*ptr), not i raw bytes —
    // mirrors the a[i] addressing math in compile_lvalue's AST_INDEX case
    // (base + index * Type_SizeOf(elem)), which this path never shared.
    // `ptr - ptr` (same pointee type) yields an element distance, so the raw
    // byte difference is divided by the pointee size after the subtract.
    if ((node->type == AST_ADD || node->type == AST_SUB) &&
        (lt->cls == TYPE_POINTER || rt->cls == TYPE_POINTER)) {
        if (lt->cls == TYPE_POINTER && rt->cls == TYPE_POINTER) {
            // ptr - ptr: rax = (rax - rcx) / esz. (ptr + ptr is not a valid op
            // and Type_Infer should already reject it before codegen is reached.)
            uint64_t esz = Type_SizeOf(lt->pointer_base);
            emit_byte(buf, 0x48); emit_byte(buf, 0x29); emit_byte(buf, 0xc8); // sub rax, rcx
            if (esz != 1) {
                // cqo ; idiv by esz (signed — byte distance is naturally signed)
                emit_byte(buf, 0x48); emit_byte(buf, 0xb9); emit_u64(buf, esz); // mov rcx, esz
                emit_byte(buf, 0x48); emit_byte(buf, 0x99);                    // cqo
                emit_byte(buf, 0x48); emit_byte(buf, 0xf7); emit_byte(buf, 0xf9); // idiv rcx
            }
            return;
        }
        // ptr +/- int, or int + ptr (commutative add only): scale the integer
        // operand by the pointee size, then add/sub raw addresses.
        Type* ptr_t = (lt->cls == TYPE_POINTER) ? lt : rt;
        uint64_t esz = Type_SizeOf(ptr_t->pointer_base);
        if (lt->cls == TYPE_POINTER) {
            // rax = pointer, rcx = integer offset -> scale rcx
            if (esz != 1) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x69); emit_byte(buf, 0xc9); emit_u32(buf, (uint32_t)esz); // imul rcx, rcx, esz
            }
            if (node->type == AST_ADD) { emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xc8); } // add rax, rcx
            else                       { emit_byte(buf, 0x48); emit_byte(buf, 0x29); emit_byte(buf, 0xc8); } // sub rax, rcx
        } else {
            // int + ptr: rax = integer, rcx = pointer -> scale rax, then add rcx
            if (esz != 1) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x69); emit_byte(buf, 0xc0); emit_u32(buf, (uint32_t)esz); // imul rax, rax, esz
            }
            emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xc8); // add rax, rcx
        }
        return;
    }

    switch (node->type) {
        case AST_ADD: emit_byte(buf, 0x48); emit_byte(buf, 0x01); emit_byte(buf, 0xc8); break;
        case AST_SUB: emit_byte(buf, 0x48); emit_byte(buf, 0x29); emit_byte(buf, 0xc8); break;
        case AST_MUL: emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xaf); emit_byte(buf, 0xc1); break;
        case AST_DIV:
        case AST_MOD:
            if (op_signed) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x99);              // cqo  (sign-extend rax->rdx)
                emit_byte(buf, 0x48); emit_byte(buf, 0xf7); emit_byte(buf, 0xf9); // idiv rcx
            } else {
                emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xd2); // xor rdx, rdx
                emit_byte(buf, 0x48); emit_byte(buf, 0xf7); emit_byte(buf, 0xf1); // div rcx
            }
            if (node->type == AST_MOD) {
                emit_byte(buf, 0x48); emit_byte(buf, 0x89); emit_byte(buf, 0xd0); // mov rax, rdx
            }
            break;
        case AST_BIT_AND: emit_byte(buf, 0x48); emit_byte(buf, 0x21); emit_byte(buf, 0xc8); break;
        case AST_BIT_OR: emit_byte(buf, 0x48); emit_byte(buf, 0x09); emit_byte(buf, 0xc8); break;
        case AST_BIT_XOR: emit_byte(buf, 0x48); emit_byte(buf, 0x31); emit_byte(buf, 0xc8); break;
        case AST_SHL:
            emit_byte(buf, 0x48); emit_byte(buf, 0xd3); emit_byte(buf, 0xe0); // shl rax, cl
            break;
        case AST_SHR:
            // arithmetic (sar 0xf8) for signed, logical (shr 0xe8) for unsigned
            emit_byte(buf, 0x48); emit_byte(buf, 0xd3);
            emit_byte(buf, op_signed ? 0xf8 : 0xe8);
            break;
        case AST_EQ:
        case AST_NEQ:
        case AST_LT:
        case AST_GT:
        case AST_LTE:
        case AST_GTE:
            emit_byte(buf, 0x48); emit_byte(buf, 0x39); emit_byte(buf, 0xc8); // cmp rax, rcx
            emit_byte(buf, 0x0f);
            switch (node->type) {
                case AST_EQ:  emit_byte(buf, 0x94); break;                       // sete
                case AST_NEQ: emit_byte(buf, 0x95); break;                       // setne
                case AST_LT:  emit_byte(buf, op_signed ? 0x9c : 0x92); break;    // setl  / setb
                case AST_GT:  emit_byte(buf, op_signed ? 0x9f : 0x97); break;    // setg  / seta
                case AST_LTE: emit_byte(buf, op_signed ? 0x9e : 0x96); break;    // setle / setbe
                case AST_GTE: emit_byte(buf, op_signed ? 0x9d : 0x93); break;    // setge / setae
                default: break;
            }
            emit_byte(buf, 0xc0);
            emit_byte(buf, 0x48); emit_byte(buf, 0x0f); emit_byte(buf, 0xb6); emit_byte(buf, 0xc0); // movzx rax, al
            break;
        default: break;
    }

    // Narrow the raw 64-bit rax result down to this operation's actual result
    // width/precision (see emit_narrow_rax_to_type). Skip comparisons
    // (AST_EQ..AST_GTE): they already self-narrow to bool via the movzx at
    // the end of their own case above.
    switch (node->type) {
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
            break;
        default:
            emit_narrow_rax_to_type(buf, Type_Infer(node));
    }
}

// Compile one top-level unit, returning the offset of its entry thunk (do not run).
size_t Backend_Compile(ASTNode* root) {
    if (s_jit_buf.code == NULL) {
        Backend_Init();
    }

    // Typechecking is now handled in the frontend by Typecheck_Program.

    // A generic function is NOT compiled here — only its instantiations are, on
    // demand. Emit nothing and hand back a sentinel the driver knows to skip.
    if (root->type == AST_FUNC_DECL && root->func_decl.type_param_count > 0) {
        return (size_t)-1;
    }

    // Extern functions have no body to compile. Skip them.
    if (root->type == AST_FUNC_DECL && root->func_decl.sym && root->func_decl.sym->is_extern) {
        return (size_t)-1;
    }

    size_t entry_offset = s_jit_buf.size;

    // Each unit is wrapped as a thunk: prologue, body, epilogue, ret.
    emit_byte(&s_jit_buf, 0x55); // push rbp
    emit_byte(&s_jit_buf, 0x48); emit_byte(&s_jit_buf, 0x89); emit_byte(&s_jit_buf, 0xe5); // mov rbp, rsp
    emit_byte(&s_jit_buf, 0x41); emit_byte(&s_jit_buf, 0x57); // push r15

    FuncContext fc = {0};
    fc.current_local_offset = 8;
    FuncContext* prev_func = s_func;
    s_func = &fc;

    compile_node_ctx(&s_jit_buf, root, NULL);

    for (size_t i = 0; i < fc.ret_count; i++) {
        size_t off = fc.ret_offsets[i];
        *(uint32_t*)(s_jit_buf.code + off) = s_jit_buf.size - (off + 4);
    }
    free(fc.ret_offsets);
    s_func = prev_func;

    emit_byte(&s_jit_buf, 0x4c); emit_byte(&s_jit_buf, 0x8b); emit_byte(&s_jit_buf, 0x7d); emit_byte(&s_jit_buf, 0xf8); // mov r15, [rbp-8]
    emit_byte(&s_jit_buf, 0x48); emit_byte(&s_jit_buf, 0x89); emit_byte(&s_jit_buf, 0xec); // mov rsp, rbp
    emit_byte(&s_jit_buf, 0x5d); // pop rbp
    emit_byte(&s_jit_buf, 0xc3); // ret

    return entry_offset;
}

// Patch all recorded call sites now that every function has a final offset.
void Backend_Finalize(void) {
    // First, drain the generic-instantiation queue. Compiling an instantiation may
    // itself enqueue further instantiations (transitive), so loop until the cursor
    // catches up. Each instantiation is compiled as an ordinary function body into
    // the JIT buffer; its synthetic symbol's offset is set so call fixups resolve.
    for (size_t i = 0; i < Generic_GetCount(); i++) {
        ASTNode* decl = Generic_GetDecl(i);
        FuncContext* prev_func = s_func;
        s_func = NULL;
        compile_node_ctx(&s_jit_buf, decl, NULL);
        s_func = prev_func;
    }

    for (size_t i = 0; i < s_fixup_count; i++) {
        Symbol* t = s_fixups[i].target;
        size_t at = s_fixups[i].patch_at;
        *(int32_t*)(s_jit_buf.code + at) = (int32_t)(t->offset - (at + 4));
    }
}

// Write a global's constexpr-folded initial value into the static image. The
// offset is the symbol's byte offset into the globals region (slots are 8-byte
// aligned). Called once per initialized global before main runs — this is the
// whole of "static initialization": no code, just the image.
void Backend_SetGlobal(int byte_offset, uint64_t value) {
    *(uint64_t*)((uint8_t*)&s_repl_globals[0] + byte_offset) = value;
}

// Write an aggregate global's folded bytes into the static image.
void Backend_SetGlobalBytes(int byte_offset, const uint8_t* bytes, uint64_t size) {
    memcpy((uint8_t*)&s_repl_globals[0] + byte_offset, bytes, size);
}

// Run a previously-compiled unit thunk by its entry offset.
uint64_t Backend_RunAt(size_t entry_offset) {
    uint64_t (*func)(void) = (uint64_t (*)(void))(s_jit_buf.code + entry_offset);
    return func();
}

void* Backend_DebugGetCodeBase(void) { return s_jit_buf.code; }

// Back-compat single-unit path (REPL): compile, finalize, run.
uint64_t Backend_CompileAndRun(ASTNode* root) {
    size_t off = Backend_Compile(root);
    Backend_Finalize();
    return Backend_RunAt(off);
}
