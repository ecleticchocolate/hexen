#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// --- Lexer ---

typedef enum {
    TOK_EOF,
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_STRING,
    TOK_IDENTIFIER,
    TOK_TRUE, TOK_FALSE, TOK_NULL,
    
    // Operators
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_MOD,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_SHL, TOK_SHR,
    TOK_EQEQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_ANDAND, TOK_OROR, TOK_BANG, TOK_TILDE,
    TOK_EQ, // =
    // Compound assignment: += -= *= /= %= &= |= ^= <<= >>=
    TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ, TOK_MOD_EQ,
    TOK_AMP_EQ, TOK_PIPE_EQ, TOK_CARET_EQ, TOK_SHL_EQ, TOK_SHR_EQ,
    
    // Punctuation
    TOK_LPAREN, TOK_RPAREN, TOK_SEMI, TOK_LBRACE, TOK_RBRACE,
    TOK_DOT, TOK_LBRACKET, TOK_RBRACKET, TOK_STRUCT, TOK_CONST, TOK_SIZEOF,
    TOK_ALIGNOF, TOK_OFFSETOF, TOK_NAMEOF,
    TOK_ENUM, TOK_UNION, TOK_MATCH, TOK_UNPACK, TOK_EXTERN, TOK_ELLIPSIS, TOK_PUB, TOK_WITH, TOK_IMPL, TOK_ALIAS,
    
    // Keywords / Types
    TOK_U8, TOK_U16, TOK_U32, TOK_U64,
    TOK_I8, TOK_I16, TOK_I32, TOK_I64,
    TOK_BOOL, TOK_F32, TOK_F64, TOK_VOID,
    
    // Control Flow
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_BREAK, TOK_CONTINUE, TOK_RETURN,
    TOK_FOR, TOK_TO, TOK_DEFER,
    
    // Functions
    TOK_FN, TOK_COMMA,
    TOK_NEW, TOK_DELETE,
    
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    uint64_t int_value;
    double   float_value; // valid when type == TOK_FLOAT
    const char* start;
    size_t length;
    int line;
    int column;
    const char* filename;
} Token;

void Lexer_Init(const char* filename, const char* source);
Token Lexer_NextToken(void);
extern bool Lexer_NewlineBefore; // true when the current token was preceded by a newline

// Scanner cursor snapshot for bounded parser lookahead (see Lexer_Save/Restore).
typedef struct {
    size_t pos;
    size_t line_pos;
    const char* line_start;
    int line;
    bool newline_before;
} LexerState;
void Lexer_Save(LexerState* st);
void Lexer_Restore(const LexerState* st);

// --- Type System ---

typedef enum {
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_FUNCTION,
    TYPE_PARAM,    // generic type-parameter placeholder (e.g. T); resolved at instantiation
    TYPE_CONST_VALUE, // A constant value sneaked into the type system for const generics
    TYPE_IMPL,     // `impl { fn name(A) B }` -- a match PATTERN only, never a real type.
                   // Asks whether the scrutinee HAS a method of that name whose
                   // signature unifies with the given fn type. Methods aren't part of
                   // the type grammar (an `impl` block is name-mangling: `p.free()` is
                   // resolved by looking up the symbol `P_free`), so this is the one
                   // pattern whose match is a SYMBOL LOOKUP rather than a structural
                   // walk of the scrutinee. Containment, not sequence: methods are an
                   // unordered set contributed by any number of `impl` blocks, so
                   // there is nothing to peel and no `Rest...`.
    TYPE_FN_LITERAL // A specific, named, non-generic top-level function used to bind a
                   // generic type parameter (`apply(3, 5, asc)` binding `Cmp` to `asc`
                   // rather than to the structural `fn(i32,i32) bool`). Identity is
                   // NOMINAL (which Symbol*), not structural (its signature) -- so
                   // `Box[fn_of(asc)]` and `Box[fn_of(desc)]` are different types even
                   // though `asc` and `desc` share a signature, the way two named
                   // structs with identical fields are still different types. Never
                   // produced by ordinary type parsing; only Type_Infer synthesizes
                   // one, at the moment a bare reference to a non-generic top-level
                   // function is used to infer a generic argument.
} TypeClass;

typedef enum {
    PRIM_U8, PRIM_U16, PRIM_U32, PRIM_U64,
    PRIM_I8, PRIM_I16, PRIM_I32, PRIM_I64,
    PRIM_BOOL, PRIM_F32, PRIM_F64, PRIM_V, PRIM_VOID
} PrimitiveKind;

typedef struct Type {
    TypeClass cls;
    // TYPE_STRUCT only: true iff this node names a still-generic template left
    // DELIBERATELY unapplied (a "template template" argument, e.g. `M` bound to
    // bare `Box` in `HKT[Box, i32]` -- Box is never instantiated here, just
    // carried as a value). Set only at that one call-site path (parser.c's
    // parse_generic_arg_list). Type_Substitute's TYPE_STRUCT case checks this to
    // avoid its `Box*` self-param auto-completion firing by NAME COINCIDENCE on a
    // node that was never meant to be completed at all.
    bool struct_unapplied;
    union {
        PrimitiveKind primitive;
        struct Type* pointer_base;
        struct {
            struct Type* element;
            uint64_t count;
            struct ASTNode* count_expr; // Unresolved array expression
            const char* size_param;     // for a `match T` array pattern `E[N]`: the name
                                        // of the size WILDCARD (`N`), when the size position
                                        // is an undeclared identifier registered as a value
                                        // param. NULL for an ordinary sized array. reflect_unify
                                        // binds this to the concrete array's count.
        } array;
        const char* struct_name;
        const char* param_name;   // for TYPE_PARAM: the type-parameter's name (e.g. "T")
        // TYPE_IMPL: the method name to look for, plus the signature it must unify
        // against. `sig` is an ordinary TYPE_FUNCTION built by parse_type, so it may
        // contain wildcards (`fn map(A) B`) -- they bind through reflect_unify exactly
        // as they do anywhere else in the type grammar. `sig` does NOT include the
        // implicit `self` parameter; the matcher strips it from the found symbol.
        struct {
            const char**  method_names;
            size_t*       method_name_lens;
            struct Type** sigs;
            size_t        method_count;
        } impl_pat;
        struct {
            struct Type* return_type;
            struct Type** param_types;
            size_t param_count;
            bool is_vararg;
        } function;
        // TYPE_CONST_VALUE: a constexpr-folded VALUE carried as a generic argument.
        // Not "an int in the type system" any more — it's a value of type `pin`,
        // produced by the same ConstEval engine that folds const globals. Scalars
        // live in `scalar`; aggregates (structs/arrays/enums) live at `agg_off` in
        // the persistent comptime arena (ConstEval_AggPersist), with `is_agg` set.
        // `pin` is the declared param kind (u32, Point, List[u32], ...), so the
        // value knows its own width/layout and wraps/compares like any pinned value.
        struct {
            int64_t      scalar;    // scalar payload (is_agg == false, defer == NULL)
            uint32_t     agg_off;   // persistent-arena offset (is_agg == true)
            struct Type* pin;       // the type this value is pinned to
            bool         is_agg;    // aggregate (arena) vs scalar payload
            struct ASTNode* defer;  // unresolved value expr referencing outer params;
                                    // re-folded by Type_Substitute at instantiation
                                    // (mirrors array.count_expr deferral). NULL when folded.
        } cval;
        // TYPE_FN_LITERAL: `sym` is the specific top-level function this literal
        // names (its identity); `sig` is the ordinary TYPE_FUNCTION underneath,
        // used by every consumer that only cares about calling convention/shape
        // (assignability to a `fn(...) T` variable, codegen, param/return checks).
        struct {
            struct Symbol* sym;
            struct Type*   sig;
        } fn_lit;
    };
} Type;

// Back-compat shim: older code reads t->const_val for the scalar payload.
#define const_val cval.scalar

// --- Struct registry ---

typedef struct StructField {
    char* name;
    Type* type;
    uint64_t offset;      // byte offset from struct base (no padding, back-to-back)
    bool has_default;     // field has a constexpr default value
    uint8_t* default_val_buf; // the folded default bytes (valid when has_default)
    // `super T base` inside a generic template, where T is still an unresolved
    // TYPE_PARAM at template-parse time: there is no struct to promote fields
    // FROM yet, so this single field (type T) is recorded as a placeholder and
    // flagged here instead. Struct_Instantiate expands it once T is bound to a
    // concrete type -- splicing that type's OWN promoted fields in at this
    // position, the same thing the non-generic `super A base` path already
    // does at parse time. Not set for an ordinary field or a `super` whose
    // type was already concrete at parse time (that one already fully
    // expanded, so instantiation just copies its promoted fields like any
    // other plain field -- nothing left to defer).
    bool is_super_param;
    // For an enum variant ONLY: its ABSOLUTE tag value (the number actually
    // stored in memory and compared by `match`/codegen). Ordinarily equal to
    // the field's own index in fields[] -- but a `Rest...` pack-tail peel on
    // an enum type (see Struct_MakeAnon) rebundles a SUFFIX of the original
    // variant list into a smaller anonymous enum, whose own fields[0] is no
    // longer tag 0. Without this, a peeled sub-enum could not tell which
    // absolute tag its local index 0 corresponds to, and every tag compare
    // downstream (Enum_VariantIndex, match's codegen, nameof) would silently
    // read the wrong variant. Meaningless (0) for a struct/union field.
    uint32_t variant_tag;
} StructField;

typedef struct StructDef {
    char* name;
    StructField* fields;     // for an enum: the VARIANTS (field.name = variant, field.type = payload, NULL if none)
    size_t field_count;      // for an enum: variant count
    uint64_t size; // total size in bytes (struct: sum of fields; enum: TAG + max payload)
    uint64_t align; // alignment requirement
    bool laid_out; // layout/cycle-check completed
    bool laying_out; // in-progress marker for cycle detection
    // An enum is a StructDef with is_enum=true: same registry/field/sizeof machinery,
    // but its "fields" are variants (one-at-a-time, overlapping) and layout is
    // TAG_SIZE + max(payload sizes) instead of the sum. field.offset is TAG_SIZE for
    // every variant (they share the payload region after the tag at offset 0).
    bool is_enum;
    // Overlapping layout with a zero-size tag: same one-region-shared-by-all-fields
    // arithmetic as is_enum, minus the discriminant. Ordinary field syntax (unlike
    // is_enum's variant syntax), so it never sets is_enum_variant/.Variant anywhere
    // -- every other is_enum-gated code path (dot-construction, match) is untouched
    // because it's keyed off is_enum specifically, not off "has overlapping layout."
    bool is_overlapping;
    bool is_pub;
    // Generics (stencil model): a generic struct is a template, never laid out.
    // Each `Name[args]` use is instantiated into a fresh concrete StructDef by
    // substituting type params, registered under a synthetic name (Name$arg...).
    bool is_generic;
    bool is_anonymous;          // true for a `struct { ... }` anonymous struct TYPE,
                                // registered under a content-derived synthetic name.
                                // A pattern-side anonymous struct destructures a
                                // concrete struct's fields positionally in reflect_unify.
    // Prototype: pack-tail field in an anonymous struct PATTERN, e.g.
    // `struct { A a  Rest... rest }`. -1 = no pack field (ordinary strict-arity
    // pattern, or a real anon struct literal type -- those never set this).
    // When >= 0, reflect_unify (reflections.c) unifies fields [0, index) positionally
    // and binds the pattern hole at `index` to a freshly synthesized anon struct
    // wrapping every concrete field from `index` onward (via Struct_MakeAnon),
    // instead of requiring exact arity. MUST be explicitly set to -1 at every
    // StructDef creation site (Struct_Register/Struct_MakeAnon use calloc, which
    // zero-inits this to 0 -- the same footgun pack_param_index hit on impl
    // methods; 0 would wrongly mean "field 0 is the pack tail").
    int pack_field_index;
    const char** type_params;   // parameter names, e.g. {"T","N"}
    struct Type** param_kinds;  // per-param kind: NULL = TYPE param, non-NULL = VALUE
                                //   param pinned to that Type (e.g. u32 for `[T, u32 N]`).
                                //   NULL for the whole array on a legacy all-type generic.
    size_t type_param_count;    // 0 for an ordinary struct
    
    // For instantiated structs (e.g. Queue$TT, Queue$p2), keep track of the base and args
    struct StructDef* generic_base;
    struct Type** type_args;
    size_t type_arg_count;
} StructDef;

StructDef* Struct_Register(const char* name, size_t len); // create or find by name
StructDef* Struct_Find(const char* name);                 // NULL if not a known struct
StructDef** Struct_GetAll(size_t* out_count);
// Append one field to a growable StructField array (doubling capacity as needed).
// `proto` supplies name/type/has_default/default_val_buf; offset always resets to
// 0 (Struct_Layout recomputes it wholesale) and is_super_param always resets to
// false (a freshly appended/spliced field is never itself a deferred super-param).
// Shared by parse_struct_decl_ex's `super` splice and Struct_Instantiate's
// generic field-copy/super-expansion, so the same five-field append isn't
// hand-written three times over.
void Struct_AppendField(StructField** fields, size_t* count, size_t* cap, StructField proto);
StructField* Struct_FindField(StructDef* sd, const char* name, size_t len);
StructField* Struct_FindField(StructDef* sd, const char* name, size_t len);
int Enum_VariantIndex(StructDef* sd, const char* name, size_t len); // tag value; -1 if not a variant
StructDef* Struct_Instantiate(StructDef* gen, Type** args, size_t argc); // generic struct instantiation
// Prototype: synthesize (or find, structurally deduped) an anonymous struct type
// from a list of already-known field types, with compiler-generated field names
// ("_0", "_1", ...). Used to bind a `T... args` pack param at a call site.
StructDef* Struct_MakeAnon(Type** field_types, size_t field_count, bool is_overlapping);
StructDef* Struct_MakeAnonEnum(Type** variant_types, uint32_t* variant_tags, size_t variant_count);
void Struct_UpdateInstantiations(StructDef* gen);
void Struct_Layout(StructDef* sd); // assign field offsets + size; errors on by-value cycle
uint64_t Type_SizeOf(const Type* t); // full storage size in bytes (basis for sizeof)
uint64_t Type_AlignOf(const Type* t); // alignment requirement in bytes
void Type_ToString(const Type* t, char* out, size_t cap); // Convert type to valid Torrent string

// --- AST ---

// lit_kind values for AST_INT_LITERAL
#define LIT_INT  0
#define LIT_BOOL 1
#define LIT_NULL 2
#define LIT_FLOAT 3
// A literal whose int_value is not an arbitrary integer but the address of a
// Symbol* (specifically a SYM_FUNCTION) — produced when a const-generic fn
// parameter is substituted with a concrete function (clone_ast, types.c).
// Tagging the literal itself (rather than relying on callers to recognize a
// specific surrounding AST shape, e.g. "cast immediately at a call site")
// means every consumer that compiles this literal to a runtime VALUE can
// look up the real JIT/extern address via emit_fn_symbol_value instead of
// emitting the raw Symbol* pointer bits as if it were a callable address.
#define LIT_FN_SYMBOL 4

typedef enum {
    AST_INT_LITERAL,
    AST_IDENT,
    AST_ADD, AST_SUB, AST_MUL, AST_DIV, AST_MOD,
    AST_BIT_AND, AST_BIT_OR, AST_BIT_XOR, AST_SHL, AST_SHR,
    AST_EQ, AST_NEQ, AST_LT, AST_GT, AST_LTE, AST_GTE,
    AST_LOGICAL_AND, AST_LOGICAL_OR, AST_LOGICAL_NOT, AST_BIT_NOT,
    AST_ASSIGN,
    AST_DECLARATION,
    AST_BLOCK,
    AST_DEREF,
    AST_ADDR,
    AST_CAST,
    AST_IF,
    AST_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_DEFER,
    AST_FOR,
    AST_FUNC_DECL,
    AST_CALL,
    AST_STRUCT_DECL,   // struct definition (no code; registered at parse time)
    AST_FIELD,         // p.x field access
    AST_STRUCT_LITERAL, // Point{.x = 1, .y = 2}
    AST_INDEX,         // a[i]
    AST_ARRAY_LITERAL, // u32[4]{1,2,3,4} or u32[]{...}
    AST_SIZEOF,        // sizeof(type) or sizeof(expr) -> u64 constant
    AST_CONST_EXPR,    // const(expr) -- forced comptime fold, DEFERRED case only.
                       // Folds to a literal at parse time in the ordinary case, so
                       // this node only survives inside a generic TEMPLATE body,
                       // where the fold must wait for a concrete instantiation.
                       // clone_ast re-folds it per instantiation (types.c).
    AST_ALIGNOF,       // alignof(type) -> u64 constant. Same shape/lifecycle as
                        // AST_SIZEOF's type-branch (reuses sizeof_expr), just
                        // backed by Type_AlignOf instead of Type_SizeOf.
    AST_OFFSETOF,       // offsetof(StructType, i) -> u64 byte offset of the
                        // i-th field (0-based, declaration order). Reuses
                        // field_ref_expr; i may reference a generic const
                        // param, resolved lazily via ConstEval exactly like
                        // AST_SIZEOF's defer_expr.
    AST_NAMEOF,         // nameof(StructType, i) -> u8* to the i-th field's
                        // name. Always resolves to a real AST_STRING before
                        // codegen -- either immediately at parse time (both
                        // type and index concrete) or in clone_ast at generic
                        // instantiation. No ConstEval/codegen case exists for
                        // this node on purpose: strings have no ConstEval
                        // representation in this compiler, so unlike
                        // AST_OFFSETOF this one MUST fold away before it
                        // could ever reach codegen.
    AST_TYPE_EXPR,     // a bare type (currently: a generic type-param like T) used
                        // directly in expression position, e.g. `T == i32`. Reuses
                        // sizeof_expr's exact shape (just node->sizeof_expr.type) —
                        // this is the same "a type sitting where an expression is
                        // expected" idea sizeof already has, minus the size
                        // computation.
    AST_STRING,        // "..." -> u8* to static NUL-terminated bytes
    AST_NEW,           // new T / new T{...} / new T[expr] -> T*
    AST_DELETE         // delete p -> free
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    struct Type* result_type; // inferred result type of this expression (NULL until computed)

    // Source location, stamped once at node creation (parser.c new_node()).
    // DEMO SCOPE: currently only used to prove out caret-style diagnostics on
    // one error site (types.c, "struct has no field"). Not yet relied on
    // anywhere else -- clone_ast's shallow `*c = *n` copy carries it through
    // generic instantiation for free, but that hasn't been verified beyond
    // this one demo path.
    int line;
    int column;
    const char* filename;

    // For AST_INT_LITERAL: distinguishes plain int / bool / null so Type_Infer
    // can hand back bool or a null-pointer type instead of i32.
    int lit_kind; // 0 = integer (default), 1 = bool, 2 = null

    union {
        uint64_t int_value;
        double   float_value; // for AST_INT_LITERAL with lit_kind == LIT_FLOAT
        struct {
            struct Type* type;       // NULL when defer_expr is set and not yet resolved
            struct ASTNode* defer_expr; // sizeof(expr)'s operand, when it references an
                                         // in-scope generic param and `type` couldn't be
                                         // inferred yet at parse time. Re-run Type_Infer
                                         // on this under the substituted frame once bound
                                         // (mirrors array.count_expr's deferral convention).
        } sizeof_expr;
        struct {
            struct ASTNode* inner;   // the operand to fold once args are concrete
        } const_expr;
        // AST_OFFSETOF / AST_NAMEOF: a struct type plus a 0-based field index.
        struct {
            struct Type* type;          // the struct type T; may still be a
                                        // generic type-param (TYPE_PARAM)
                                        // until instantiation
            struct ASTNode* index_expr; // 0-based field index; a literal or a
                                        // reference to an in-scope generic
                                        // const param. AST_OFFSETOF resolves
                                        // this lazily via ConstEval under the
                                        // active substitution frame, exactly
                                        // like sizeof_expr.defer_expr.
                                        // AST_NAMEOF instead always resolves
                                        // it eagerly (parse time, or in
                                        // clone_ast at instantiation) and
                                        // rewrites itself into an AST_STRING,
                                        // since a field NAME has no
                                        // ConstEval/codegen representation.
        } field_ref_expr;
        struct {
            const char* name;
            size_t name_len;
            struct Symbol* sym;
            // Set when a generic function name is used in value position
            // (assigned to a concrete fn(...) type). Backend calls
            // get_instantiation(sym, type_args, type_arg_count) instead of
            // emitting a bare fixup to the uninstantiated generic symbol.
            struct Type** type_args;
            size_t type_arg_count;
        } ident;
        struct {
            struct ASTNode* left;
            struct ASTNode* right;
            bool is_compound; // AST_ASSIGN only: came from `a op= b` desugar, so
                               // `left` is a shared subtree (also pointed to by
                               // right's binop) -- codegen must spill its address
                               // instead of walking it twice. See compile_lvalue's
                               // memo in backend_x64.c.
        } binary;
        struct {
            struct Type* var_type;
            const char* name;
            size_t name_len;
            struct ASTNode* init_expr; // Can be NULL
            struct Symbol* sym;
            bool is_generic_const; // `const` decl deferred from parse-time folding
                                    // because its initializer mentions an in-scope
                                    // generic param; must be re-folded per-instantiation
                                    // in clone_ast (see backend_x64.c AST_DECLARATION case).
        } decl;
        struct {
            struct ASTNode** statements;
            size_t count;
            size_t capacity;
            bool transparent; // no new comptime-local scope on entry/exit (e.g. the
                               // synthetic wrapper `unpack` desugars into) -- bindings
                               // made inside are meant to escape into the enclosing
                               // scope, matching the symbol-table behavior parse_unpack
                               // already relies on for the real (non-const) path.
        } block;
        struct ASTNode* unary;
        struct {
            struct Type* target_type;
            struct ASTNode* expr;
            size_t struct_downcast_offset;
        } cast;
        struct {
            struct ASTNode* condition;
            struct ASTNode* true_block;
            struct ASTNode* false_block; // Can be NULL
            // `match T` arm lowering (parser.c parse_match_type): when set, this
            // AST_IF is a TYPE-PATTERN arm, not an ordinary if. The condition is a
            // placeholder; branch selection instead runs reflect_unify(scrut_type,
            // reflect_pattern). On a match, the collected wildcard bindings are
            // substituted into true_block (via clone_ast) so pattern holes (`P`,
            // `E`, `N`) become concrete in the arm body — reusing the exact
            // monomorphization transform, no new mechanism. NULL when this is a
            // plain if. (An `else` arm has reflect_pattern == NULL but sits in the
            // false_block chain like any other match, so it needs no marker.)
            struct Type* reflect_pattern;    // the arm's pattern type (may contain TYPE_PARAM holes)
            struct Type* reflect_scrutinee;  // the type being matched on
            // Set on the LAST link of an if-chain that a `match` lowered to, when that
            // match was verified exhaustive with no `else` arm (every enum variant
            // covered, or both bools). The lowering keeps a condition on that final arm
            // (`else if (tag == 2) { C }`), so the node has no false_block even though
            // the arm is, in fact, unconditional. Only the "does every path return?"
            // check (types.c, stmt_always_returns) reads this -- codegen is unchanged.
            bool exhaustive_tail;
        } if_stmt;
        struct {
            struct ASTNode* condition;
            struct ASTNode* body;
        } while_stmt;
        struct {
            struct ASTNode* init;   // loop-var declaration (i = A)
            struct ASTNode* cond;   // i < B
            struct ASTNode* incr;   // i = i + 1  (continue lands HERE)
            struct ASTNode* body;
        } for_stmt;
        struct {
            const char* name;
            size_t name_len;
            struct Symbol* params;
            struct Symbol** param_syms; // resolved param symbols (offset + type), in order
            size_t param_count;
            struct Type* return_type;
            struct ASTNode* body;
            struct Symbol* sym;
            // Generics (stage 1: functions only). A generic function is NOT compiled
            // at definition — its AST is kept and each [T] instantiation is cloned,
            // type-substituted, and compiled on demand.
            const char** type_params;   // parameter names, e.g. {"T","N"}
            struct Type** param_kinds;  // per-param kind: NULL = TYPE param, non-NULL =
                                        //   VALUE param pinned to that Type. NULL array
                                        //   for a legacy all-type generic function.
            size_t type_param_count;    // 0 for an ordinary (non-generic) function
            // Prototype pack support: index of a `T... name` value-parameter in
            // param_syms, or -1 if this function has none. At most one, and it
            // must be the last value-parameter (enforced at parse time).
            int pack_param_index;
        } func_decl;
        struct {
            const char* target_name;
            size_t target_name_len;
            struct ASTNode* target_expr;
            struct ASTNode** args;
            size_t arg_count;
            struct Symbol* sym;
            struct Type** type_args;    // explicit [T,...] at the call site (generics)
            size_t type_arg_count;      // 0 for a call to a non-generic function
            // impl method call on an instantiated generic struct (Box[i32].get()):
            // the struct already fixes a PREFIX of the method's type params (e.g. T),
            // but the method may declare its OWN extra ones (e.g. fn map[U](...)).
            // self_type_args holds just that known prefix so infer_generic can seed
            // inferred_args[0..self_type_arg_count) and still infer the rest (U) from
            // arguments/return context, instead of the all-or-nothing type_args above.
            struct Type** self_type_args;
            size_t self_type_arg_count;
            bool pack_rewritten; // prototype: trailing args already bundled into one anon-struct arg
            bool index_deref_wrapped; // v[i] -> v.__index(i) already wrapped in its AST_DEREF
                                      // (see wrap_index_result_deref, types.c) -- set on the MOVED
                                      // call_copy so a later, redundant visit (the wrapper's own
                                      // Typecheck_Tree(node->unary) walk re-entering infer_generic
                                      // on this same call_copy) doesn't wrap it a second time.
        } call;
        struct {
            struct ASTNode* base;      // the struct expression (or pointer, auto-derefed once)
            const char* field_name;
            size_t field_name_len;
            struct StructField* field; // resolved during typecheck
            struct StructDef* sdef;    // owning struct def
        } field;
        struct {
            struct StructDef* sdef;
            const char** field_names;   // designated field names
            size_t* field_name_lens;
            struct ASTNode** values;
            size_t count;
            // When sdef==NULL this is a deferred literal resolved against the
            // target type in typecheck. is_enum_variant distinguishes a deferred
            // ENUM literal (`.Variant{payload}`: field_names[0]=variant name,
            // count=payload count 0/1) from a deferred STRUCT literal
            // (`{.field=..}`: field_names=designated fields). Ignored once sdef
            // is filled in (a resolved enum literal sets sdef + is_enum_variant).
            bool is_enum_variant;
        } struct_lit;
        struct {
            struct ASTNode* base;  // the array (or pointer) being indexed
            struct ASTNode* index; // integer index expression
        } index;
        struct {
            struct Type* elem_type;     // element type
            struct ASTNode** values;
            size_t count;
        } array_lit;
        struct {
            struct Type* alloc_type;    // the T in `new T` (element type for arrays)
            struct ASTNode* init;       // optional {...} struct/array literal, else NULL
            struct ASTNode* count;      // optional [expr] runtime count, else NULL (single)
        } new_expr;
        struct {
            struct ASTNode* ptr;        // the pointer expression to free
        } delete_expr;
    };
} ASTNode;

// --- Symbol Table ---

typedef enum {
    SYM_GLOBAL,
    SYM_LOCAL,
    SYM_FUNCTION
} SymbolKind;

typedef struct Symbol {
    const char* name;
    size_t name_len;
    Type* type;
    SymbolKind kind;
    bool is_extern;
    int offset; // For global, offset in globals array. For local, rbp offset. For func, JIT buffer offset.
    // For SYM_GLOBAL scalars: the constexpr-folded initial value, written into the
    // static global image before main runs. Globals take a constant expression
    // initializer (same ConstEval engine as array sizes, for-step, field defaults,
    // and named consts) — never runtime code, so there is no pre-main init phase.
    int64_t global_init;
    bool    has_init;
    // For an AGGREGATE global with an initializer: the folded bytes (struct/array
    // image). has_init is also set; global_bytes!=NULL distinguishes it from the
    // scalar global_init path. Size is Type_SizeOf(sym->type).
    uint8_t* global_bytes;
    bool is_pub;
    // For a generic function symbol (SYM_FUNCTION with type params): a pointer to
    // its AST_FUNC_DECL node, kept so the backend can clone+substitute+compile each
    // instantiation. NULL for ordinary functions.
    struct ASTNode* generic_decl;
    // For ANY function symbol: a pointer to its AST_FUNC_DECL, so the constexpr
    // evaluator can interpret the body at compile time (Option A: functions are
    // constexpr-callable without marking; the call site's context drives it).
    struct ASTNode* func_decl;
    // Prototype: `T... name` value-parameter (variadic type pack). This param
    // slot is filled at each call site by bundling every trailing call argument
    // into ONE synthesized anonymous-struct value (see pack_expand_call_args in
    // types.c) -- so downstream (arity checks, ABI, codegen) never sees a pack,
    // only an ordinary struct-typed argument.
    bool is_pack;
    // For a struct-typed const-generic param materialized as a synthetic global
    // (types.c's materialize_agg_param, `$cgen$L$0`): the comptime interpreter's
    // OWN cached copy of global_bytes, persisted into s_ce_mem once on first
    // read (constexpr.c's ce_eval_ident) rather than re-copied fresh on every
    // read. -1 = not yet materialized this compiler run. Without this cache, a
    // value like `L` read many times across many separate comptime-call frames
    // (e.g. once per for-in loop iteration) allocated a fresh, independent arena
    // copy every single time -- wasteful, and the real root cause of a found
    // corruption bug where an EARLIER loop's accumulator silently changed value
    // after a LATER, unrelated-looking loop ran on a different generic
    // instantiation.
    int64_t ce_cached_addr;
} Symbol;

typedef struct SymbolTable {
    Symbol** symbols;
    size_t count;
    size_t capacity;
    struct SymbolTable* parent;
    int current_global_offset;
    int current_local_offset;
    bool is_function_scope;
} SymbolTable;

SymbolTable* SymTable_Create(SymbolTable* parent);
Symbol* SymTable_Add(SymbolTable* table, const char* name, size_t len, Type* type, SymbolKind kind);
Symbol* SymTable_Find(SymbolTable* table, const char* name, size_t len);
SymbolTable* Get_SymTable(void);

// --- Constexpr / named constants ---
//
// One evaluator, three use sites (spec): array sizes, global initializers,
// field defaults. Constexpr = integer arithmetic/bitwise/comparison over literals
// and named constants; no runtime values, calls, or variable reads.

typedef struct {
    char* name;
    int64_t value;
    Type* type;
    bool is_pub;
    // Deferred const eval: if the initializer couldn't fold at parse time (e.g. it
    // calls a function defined LATER in the file), we stash its AST here and retry
    // after the whole file is parsed (Const_ResolvePending). NULL once resolved.
    struct ASTNode* pending_expr;
} ConstDef;

ConstDef* Const_Register(const char* name, size_t len, int64_t value, Type* type);
ConstDef* Const_Find(const char* name, size_t len);
ConstDef* Const_GetAll(size_t* out_count);
// Resolve any consts whose initializer was deferred (forward-referenced a later
// fn). Returns false (and reports) if one still can't fold. Call after parsing.
bool Const_ResolvePending(void);

// ─── globals needing byte-baking, regardless of which scope they were declared in ──
// A `const` AGGREGATE is emitted as read-only global storage (it needs an address,
// unlike a scalar const, which splices a literal at each use site). But it can be
// *written* at function scope, in which case its Symbol lives in that function's
// table, not the global one — so the image initializer, which walks only the global
// table, never saw it and left the slot zeroed (silent wrong values).
//
// Membership in a scope table is therefore the wrong thing to key emission on.
// Instead, every symbol that carries `global_bytes` records itself here at the
// moment the bytes are folded, and the emitter drains this list. Scope-agnostic by
// construction — a global-scope const registers here too, so there is exactly one
// path, not a special case for the local one.
void     Global_RegisterForEmit(Symbol* sym);
size_t   Global_EmitCount(void);
Symbol*  Global_EmitAt(size_t i);
void Const_RegisterPendingUse(struct ASTNode* node, ConstDef* cdef); // back-patch inlined uses
size_t Const_PendingUseCount(void);

// Fold an AST expression to a compile-time integer. Returns true on success;
// false means "not a constant expression" (the caller reports the error site).
bool ConstEval(ASTNode* node, int64_t* out);
extern const char** s_ce_generic_params;
extern struct Type** s_ce_generic_args;
extern size_t s_ce_generic_n;
extern bool s_ce_isfloat;
extern bool s_ce_isfnsym;
bool ConstEval_Bytes(ASTNode* node, uint8_t* out_buf, uint64_t size); // aggregate const -> raw bytes
int64_t ConstEval_AggPersist(struct ASTNode* node, Type* t); // aggregate const -> persistent arena offset
bool ConstEval_ReadBytes(uint32_t off, uint8_t* out_buf, uint64_t size); // copy persisted arena bytes out
bool ConstEval_AggHasEscapingPtr(Type* t, uint8_t* bytes, uint64_t size); // const stores a live comptime pointer?
void try_rewrite_method_call(struct ASTNode* node); // rewrite expr.method(args) -> Base_method(&expr, args)
// (type, method-name) -> symbol name / Symbol. The single mangling used by method
// CALLS, `impl {...}` MATCHING, and `fnof`, so all three agree. Resolves through
// generic_base, which is why it is not nameof+concat.
char*          Method_Mangle(const struct Type* t, const char* mname, size_t mlen, size_t* out_len);
struct Symbol* Method_Resolve(const struct Type* t, const char* mname, size_t mlen);

// Operator-overload rewrites (types.c): `a op b` -> `a.__op(b)` (binary),
// `v[i] -> v.__index(i)`, `!v/~v -> v.__not()/__bitnot()`, and `a(x) ->
// a.__call(x)` (the last needs the caller to have already inferred the call
// target's type, `tgt`) -- each mutates its node in place when it applies.
// Shared by Type_Infer, infer_generic, and ConstEval (constexpr.c) -- one
// implementation each, not one per consumer of the AST.
bool try_rewrite_operator_method(struct ASTNode* node);
bool try_rewrite_index_method(struct ASTNode* node);
bool try_rewrite_unary_operator_method(struct ASTNode* node);
bool try_rewrite_call_operator(struct ASTNode* node, struct Type* tgt);
bool try_rewrite_cast_operator(struct ASTNode* node);

// --- Type helpers ---

bool Type_Equals(const Type* a, const Type* b); // structural equality (recurses through pointers/arrays)
bool Type_IsSigned(const Type* t);   // signed integer? (i8..i64). pointers/unsigned/bool => false
bool Type_IsFloat(const Type* t);    // f32 or f64?
bool Type_IsAggregate(const Type* t); // struct or array? (addressed, not register-held)
// "No value" -- an OMITTED type (NULL: an omitted fn return, a no-payload enum
// variant) or an EXPLICIT void/PRIM_V node. Two spellings of the same thing.
bool Type_IsVoidLike(const Type* t);
Type* Type_Substitute(Type* t, const char** params, Type** args, size_t n);
Type* Type_Substitute_Through_Instance(Type* t, StructDef* sd);
Type* Type_FnLitShape(Type* t); // unwrap TYPE_FN_LITERAL to its underlying TYPE_FUNCTION shape; no-op otherwise
int  Type_Width(const Type* t);      // size in bytes of the value in a register context (1,2,4,8)
// Shared width table for a REAL (non-void) primitive kind: 1/2/4/8, or -1 for
// PRIM_V/PRIM_VOID (caller must decide what void means in their own context --
// Type_Width and Type_SizeOf deliberately answer that differently; see both).
int Prim_Width(PrimitiveKind p);
bool Type_IntLiteralFits(int64_t v, const Type* dst); // same fit rule check_assignable uses for literals
Type* Type_Infer(ASTNode* node);     // result type of an expression (memoized in node->result_type)
// Bottom-up/top-down generic type-arg inference for an unresolved generic call
// or bare fn-ident reference (target may be NULL for AST_CALL; required for
// AST_IDENT). Self-contained: only reads already-inferrable argument types and
// the callee's static signature, and only writes node->call.type_args /
// node->ident.type_args on success. Exposed (was static) so ConstEval can
// reuse the exact same inference the ordinary typecheck pass uses, instead of
// refusing to fold any generic call whose type args aren't already explicit.
void infer_generic(ASTNode* node, Type* target);
Type* Type_MakePrim(int primitive_kind); // construct a primitive Type* (PrimitiveKind cast to int at the call site)

// --- Reflections: structural type unification for `match T` (reflections.c) ---
//
// A growable (name -> concrete Type*) list, in exactly the shape clone_ast /
// Type_Substitute consume for monomorphization. reflect_unify fills it by walking
// a concrete scrutinee type against a pattern type; the caller then substitutes
// the taken match arm's body against these bindings, so pattern wildcards (`P`
// in `P*`, `E`/`N` in `E[N]`) become concrete in the arm — the same transform a
// generic call applies to a function body, reused rather than reinvented.
typedef struct ReflectBindings {
    const char** names;
    struct Type** args;
    size_t count;
    size_t capacity;
} ReflectBindings;

// Unify `concrete` against `pattern`, collecting wildcard bindings into `out`.
// Returns true iff the shapes match structurally all the way down. A false result
// is what drives dead-branch elimination for a non-matching `match T` arm.
bool reflect_unify(struct Type* concrete, struct Type* pattern, ReflectBindings* out);
void reflect_bindings_free(ReflectBindings* b);

void AST_Dump(struct ASTNode* node, int depth);
void  Typecheck_Tree(ASTNode* root); // eager pass: annotate every node's result_type (home for v1(b) checks)
void  resolve_brace_literal(ASTNode* node, Type* target); // bind a bare `{...}` literal to its context type

// --- Parser ---

void Parse_Init(const char* filename, const char* source);
ASTNode* Parse_Block(void);
bool Parse_HadError(void); // true if any Parse_Block hit a parse error (file mode should abort)

// Pass 0b -- signature-only pre-parse. Reads the HEADER of every top-level `struct`,
// `enum` and `fn` (name + generic parameter list) and skips the body, so that by the
// time Pass 1 parses a use site, every declaration's `param_kinds` are already known
// regardless of source order.
//
// This exists because an explicit generic argument list is parsed KIND-DRIVEN: to read
// `f[T, 4]` the parser must know whether each bracket slot is a TYPE or a VALUE, and the
// only source of that is the callee's declaration. Without this pass, a forward use
// (`b[T]()` above `fn b[T]`) had no kinds to consult and failed -- while a plain forward
// call `b()` worked fine, because ordinary calls resolve late in typecheck. The
// asymmetry was an omission, not a design constraint: the lexer-only prescan that
// preceded this could only count arity (it has no type parser, so it cannot build the
// `Type*` pin a value slot needs), and so it stopped short of recording kinds.
//
// Crucially this does NOT reimplement the type-vs-value rule: it calls the same
// `parse_generic_param_list` the real parse uses, so there is exactly one definition of
// "bare ident = type slot, `type ident` = value slot" and it cannot drift. Pass 1
// recomputes the same values from the same function, so the two can never disagree.
void Parse_Signatures(const char* filename, const char* source);

// --- Backend ---
extern bool g_aot_mode;

uint64_t Backend_CompileAndRun(ASTNode* root); // REPL: compile+finalize+run one unit
size_t   Backend_Compile(ASTNode* root);       // file mode: compile a unit, return entry offset
void     Backend_Finalize(void);               // patch all call fixups (after all compiles)
uint64_t Backend_RunAt(size_t entry_offset);   // run a compiled unit by entry offset
void     Backend_SetGlobal(int byte_offset, uint64_t value); // write static global image
void     Backend_SetGlobalBytes(int byte_offset, const uint8_t* bytes, uint64_t size); // aggregate global
void     Backend_EmitELF(const char* output_filename); // AOT: wrap the JIT buffer in an ELF object
// Monomorphize a generic function/method: clone+substitute its declaration
// against concrete type args, caching by (generic, args) so repeat call
// sites reuse the same instantiation. Pure AST transformation (clone_ast),
// no codegen — despite living in backend_x64.c, it has no JIT dependency,
// which is what lets constexpr.c call it directly for comptime generic calls.
struct Symbol* Generic_Instantiate(struct Symbol* generic, struct Type** targs, size_t targ_count);
size_t Generic_GetCount(void);
struct ASTNode* Generic_GetDecl(size_t index);
void Typecheck_Program(struct ASTNode** units, size_t count);

// Pure AST clone with generic-param substitution (backend_x64.c). Declared here
// (not just file-local) so other translation units — types.c's `match T` arm
// selection, which substitutes wildcard bindings into the taken body — get the
// correct ASTNode* return type. Without a visible prototype, C defaults the
// return to int and truncates the 64-bit pointer to garbage.
struct ASTNode* clone_ast(struct ASTNode* n, const char** params, struct Type** args, size_t np, bool clone_symbols);

// --- Extern / FFI ---
void* Extern_Resolve(const char* name);

// --- Modules ---
void Module_Generate(const char* out_filename);

// --- Diagnostics (error.c) ---
// Shared caret-style error reporting. unwind_buf == NULL means "no recovery
// point exists here, print and exit(1)" (typecheck, codegen). A non-NULL
// unwind_buf longjmps back to the caller after printing instead (parser.c's
// two setjmp sites need this: Parse_Signatures' pre-pass deliberately
// swallows and re-parses rather than reporting twice).
#include <setjmp.h>
void Error_AtToken(Token tok, const char* msg, jmp_buf* unwind_buf);
void Error_AtNode(struct ASTNode* node, const char* msg, jmp_buf* unwind_buf);

#endif