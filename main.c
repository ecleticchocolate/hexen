#define _GNU_SOURCE
#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

// LLVM IR text backend implemented in Torrent (backend_llvm.t)

// Pre-scan pass: register every struct/enum name (and its generic arity) before
// any file is fully parsed. Runs purely through the lexer so it doesn't touch
// parser state. After this pass, Struct_Find() will succeed for any type name
// used in field types or return types, eliminating forward-reference failures.
static void prescan_structs(const char* filename, const char* source) {
    Lexer_Init(filename, source);
    Token t = Lexer_NextToken();
    while (t.type != TOK_EOF) {
        // skip 'pub' if present
        if (t.type == TOK_PUB) t = Lexer_NextToken();
        if (t.type != TOK_STRUCT && t.type != TOK_ENUM) { t = Lexer_NextToken(); continue; }
        bool is_enum = (t.type == TOK_ENUM);
        t = Lexer_NextToken();
        if (t.type != TOK_IDENTIFIER) continue;

        // Register the name (idempotent if already registered)
        StructDef* sd = Struct_Register(t.start, t.length);
        // Record is_enum up front, same as arity below: Struct_Instantiate copies
        // gen->is_enum onto every instantiation AT THE MOMENT it's created, which can
        // happen while parsing an earlier file, before this type's own `enum`/`struct`
        // keyword is reached by the real (pass 1) parser. Left false (calloc's
        // default), an instantiation created too early baked in is_enum=false
        // permanently -- pass 1 later re-sets the GENERIC BASE's is_enum correctly,
        // but nothing ever revisits instantiations made before that point. Setting it
        // here closes that window; pass 1 re-sets the same value on the base, so this
        // is idempotent, not a second source of truth.
        sd->is_enum = is_enum;
        t = Lexer_NextToken();

        // Generic type-parameter list: struct Name[T, U] { ... }
        // Only record arity + is_generic — do NOT set type_params or install
        // s_type_params. The real parser will do that with proper scoping.
        // Setting type_params here would cause T/K/V to be treated as registered
        // structs and mis-instantiate generics inside field types.
        if (t.type == TOK_LBRACKET) {
            size_t pcount = 0;
            t = Lexer_NextToken();
            while (t.type != TOK_RBRACKET && t.type != TOK_EOF) {
                if (t.type == TOK_IDENTIFIER) pcount++;
                t = Lexer_NextToken();
                if (t.type == TOK_COMMA) t = Lexer_NextToken();
            }
            sd->is_generic = (pcount > 0);
            sd->type_param_count = pcount;
            if (t.type == TOK_RBRACKET) t = Lexer_NextToken();
        }

        // Skip the body by counting braces
        if (t.type == TOK_LBRACE) {
            int depth = 1;
            t = Lexer_NextToken();
            while (t.type != TOK_EOF && depth > 0) {
                if (t.type == TOK_LBRACE) depth++;
                else if (t.type == TOK_RBRACE) depth--;
                if (depth > 0) t = Lexer_NextToken();
            }
        }
        t = Lexer_NextToken();
    }
}

static int   s_argc = 0;
static char** s_argv = NULL;
int   __argc(void) { return s_argc; }
char** __argv(void) { return s_argv; }

int main(int argc, char** argv) {
    // Raise stack limit to 256MB for deeply recursive interpreted programs.
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0) {
        rl.rlim_cur = 256 * 1024 * 1024;
        setrlimit(RLIMIT_STACK, &rl);
    }

    // Split on "--": everything after is the app's argv (argv[0] = the source file)
    s_argc = 1;
    s_argv = argv; // default: just argv[0] (no app args)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            s_argc = argc - i; // count from "--" position, shifting so [0]="--"
            s_argv = argv + i; // argv[0]="--", argv[1]=first app arg
            // make argv[0] look like the program name by reusing it
            s_argv[0] = argv[0];
            argc = i; // compiler only sees args before "--"
            break;
        }
    }
    if (argc >= 2) {
        const char* emit_mod_file = NULL;
        bool do_aot = false;
        const char* aot_file = "out.o";
        const char* llvm_file = NULL;   // -llvm <path>: emit LLVM IR text, skip JIT
        
        size_t cap = 64, count = 0;
        ASTNode** units = malloc(cap * sizeof(ASTNode*));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-emit-mod") == 0) {
            if (i + 1 < argc) emit_mod_file = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-c") == 0) {
            do_aot = true;
            g_aot_mode = true;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) aot_file = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-llvm") == 0) {
            if (i + 1 < argc) llvm_file = argv[++i];
            continue;
        }

        // (skip — file args are handled in the two-pass loops below)
        (void)argv[i]; // suppress unused warning; real loop starts after
    }

    // Collect all source files and their contents first
    size_t file_cap = 64, file_count = 0;
    const char** file_names   = malloc(file_cap * sizeof(char*));
    char**        file_sources = malloc(file_cap * sizeof(char*));
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-emit-mod") == 0) { i++; continue; }
        if (strcmp(argv[i], "-c")        == 0) continue;
        if (strcmp(argv[i], "-o")        == 0) { i++; continue; }
        if (strcmp(argv[i], "-llvm")     == 0) { i++; continue; }

        const char* input_file = argv[i];
        FILE* f = fopen(input_file, "rb");
        if (!f) { perror("fopen"); return 1; }
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);

        if (file_count >= file_cap) { file_cap *= 2; file_names = realloc(file_names, file_cap * sizeof(char*)); file_sources = realloc(file_sources, file_cap * sizeof(char*)); }
        file_names[file_count]   = input_file;
        file_sources[file_count] = source;
        file_count++;
    }

    // Pass 0a: lexer pre-scan — register all struct/enum NAMES so that parse_type()
    // in pass 0b can resolve a type name that is declared later in the file.
    for (size_t i = 0; i < file_count; i++)
        prescan_structs(file_names[i], file_sources[i]);

    // Pass 0b: signature pre-parse — record every generic declaration's param KINDS
    // (which slot is a type, which is a value) by running the real parser over headers
    // only. An explicit generic argument list is parsed kind-driven, so a use site needs
    // the callee's kinds; without this, a forward `b[T]()` had nothing to consult and
    // failed, even though a plain forward `b()` has always worked (ordinary calls resolve
    // late, in typecheck). This closes that asymmetry for functions AND structs at once,
    // reusing parse_generic_param_list rather than duplicating the rule.
    for (size_t i = 0; i < file_count; i++)
        Parse_Signatures(file_names[i], file_sources[i]);

    // Pass 1+2: full parse + compile (existing logic unchanged)
    for (size_t i = 0; i < file_count; i++) {
        Parse_Init(file_names[i], file_sources[i]);
        while (1) {
            ASTNode* ast = Parse_Block();
            if (!ast) break;
            if (count >= cap) { cap *= 2; units = realloc(units, cap * sizeof(ASTNode*)); }
            units[count++] = ast;
        }
    }
        // A parse error mid-file must abort the whole compile -- otherwise the partial
        // program (everything before the error) would silently run.
        if (Parse_HadError()) {
            return 1;
        }

        if (!Const_ResolvePending()) {
            return 1;
        }

        // --- Pass 1: Typecheck the whole program, which resolves all generics ---
        Typecheck_Program(units, count);

        if (emit_mod_file) {
            Module_Generate(emit_mod_file);
        }

        // LLVM IR text backend: emit and exit, bypassing the x86 JIT entirely.
        if (llvm_file) {
#ifndef STAGE1
            extern void torrent_backend_llvm(struct ASTNode** units, size_t unit_count, const char* out_path);
            torrent_backend_llvm(units, count, llvm_file);
#else
            printf("Error: Stage 1 compiler cannot emit LLVM IR. Bootstrap first.\n");
#endif
            free(units);
            return 0;
        }

        // Pass 2: compile every unit (all function signatures already registered
        // during parsing, so calls to later-defined functions resolve fine).
        size_t* entries = malloc(count * sizeof(size_t));
        for (size_t i = 0; i < count; i++) entries[i] = Backend_Compile(units[i]);
        // Pass B: patch all call sites.
        Backend_Finalize();
        
        if (do_aot) {
            Backend_EmitELF(aot_file);
            free(entries);
            free(units);
            return 0;
        }

        // Initialize the static global image from the constexpr-folded values
        // recorded on each global symbol. This is the entirety of static
        // initialization — no code runs, the values are simply placed. There is
        // no pre-main initialization phase and thus no init-order to get wrong.
        SymbolTable* gt = Get_SymTable();
        for (size_t i = 0; i < gt->count; i++) {
            Symbol* s = gt->symbols[i];   // symbols[] is Symbol*[]; element IS the symbol
            if (s->kind == SYM_GLOBAL && s->has_init) {
                if (s->global_bytes)
                    Backend_SetGlobalBytes(s->offset, s->global_bytes, Type_SizeOf(s->type));
                else
                    Backend_SetGlobal(s->offset, (uint64_t)s->global_init);
            }
        }
        // Plus every symbol carrying folded bytes that ISN'T in the global table --
        // i.e. a `const` aggregate declared inside a function body. It has global
        // storage (SYM_GLOBAL offset) but lives in its function's scope table, so the
        // walk above cannot see it. Emission must key on "has bytes", not on scope.
        // Idempotent: a global-scope const registers here too and re-writing identical
        // bytes to the same offset is a no-op.
        for (size_t i = 0; i < Global_EmitCount(); i++) {
            Symbol* s = Global_EmitAt(i);
            if (s && s->kind == SYM_GLOBAL && s->has_init && s->global_bytes)
                Backend_SetGlobalBytes(s->offset, s->global_bytes, Type_SizeOf(s->type));
        }
        // main is the entry point. (Compiling an fn-decl unit only DEFINES the
        // function — its thunk emits a jmp-over-body then xor rax,rax — so it must
        // be invoked explicitly here.) Fall back to running units in order only
        // when there is no main (e.g. a file of bare top-level expressions).
        Symbol* main_sym = SymTable_Find(gt, "main", 4);
        if (main_sym && main_sym->kind == SYM_FUNCTION && !main_sym->generic_decl && !main_sym->is_extern) {
            fprintf(stderr, "Found main! running... offset=%lu\n", (size_t)main_sym->offset);
            // Print the result as a 64-bit SIGNED int so negatives show correctly
            // (e.g. -7 from `-7 % 3`) AND the full i64 range is preserved (i64 max,
            // 3_000_000_000, etc). The runner can't recover the result's signedness
            // from a raw u64; signed-64 is the most useful interpretation (a genuine
            // u64 > 2^63 would display negative, which is rare and unavoidable here).
            printf("= %lld\n", (long long)Backend_RunAt((size_t)main_sym->offset));
        } else {
            if (main_sym && main_sym->kind == SYM_FUNCTION) {
                fprintf(stderr, "main found but it is generic (or uncompiled), falling back...\n");
            }
            fprintf(stderr, "main not found or generic, running bare expressions... count=%zu\n", count);
            for (size_t i = 0; i < count; i++) {
                if (entries[i] == (size_t)-1) continue; // generic decl: nothing to run
                printf("= %lld\n", (long long)Backend_RunAt(entries[i]));
            }
        }
        free(entries);
        free(units);
        return 0;
    }

    // No files passed, enter REPL mode
    printf("Hexen Compiler (v1)\n");
    printf("Enter an arithmetic expression (or 'quit' to exit)\n");

    char line[1024];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        if (strncmp(line, "quit", 4) == 0) {
            break;
        }

        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        Parse_Init("<stdin>", line);
        ASTNode* ast = Parse_Block();
        
        if (ast) {
            uint64_t result = Backend_CompileAndRun(ast);
            printf("= %lu\n", result);
        }
        
        // TODO: Free AST to prevent memory leak in REPL
    }

    return 0;
}