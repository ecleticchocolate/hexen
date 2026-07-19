#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

// Generic dynamic-array push: grows `arr` (realloc, doubling) when `count`
// reaches `cap`, then appends `item`. Handles cap==0 (initial allocation).
#define DA_PUSH(arr, count, cap, item) do { \
    if ((count) >= (cap)) { (cap) = (cap) ? (cap)*2 : 4; \
        (arr) = realloc((arr), (cap)*sizeof(*(arr))); } \
    (arr)[(count)++] = (item); \
} while (0)

static ASTNode* parse_statement(void);
static ASTNode* parse_top_level(void);
static ASTNode* parse_impl_block(bool is_pub);
// impl_type_name/impl_type_len/impl_sd are non-NULL/non-zero only when parsing
// a method inside an `impl TYPE { ... }` block: the parsed fn is then mangled
// to `TYPE_method`, gets an injected `TYPE* self` first parameter, and its
// generic scope is seeded from the struct's own type params before its own
// (optional) `[...]` extension is parsed. impl_type_name must outlive the
// call (it's stored on the AST and on self's Type) -- callers pass the
// original, non-freed token text.
static ASTNode* parse_fn_decl(bool is_pub, bool is_extern,
                              const char* impl_type_name, size_t impl_type_len,
                              StructDef* impl_sd);
static ASTNode* parse_expr_prec(int min_prec);
static ASTNode* parse_block_body(void);
static ASTNode* parse_alias_decl(void);
static ASTNode* parse_const_decl(bool is_pub);
static ASTNode* parse_const_block(void);
static void advance(void);
static void parse_error(const char* msg);
static bool token_is_type_start(TokenType t);
static bool curr_begins_type(void); // token_is_type_start + registered struct/generic names
static Type* parse_type(void);
static void parse_generic_param_list(const char*** names_out, Type*** kinds_out, size_t* count_out);
static Type* parse_generic_value_arg(Type* pin);
static void parse_generic_arg_list(const char** param_names, Type** param_kinds, size_t param_count,
                                    Type*** out_targs, size_t* out_tcount);
static ASTNode* make_ident_node(const char* name, size_t len, Symbol* sym);
static Type* make_const_value_type(Type* pin);
// Generic FUNCTION headers recorded by pass 0b (Parse_Signatures). Defined here rather
// than at the pass itself because the explicit-generic-call site, far above, must read
// its fields to classify a forward call's bracket list.
struct GenericSig {
    const char*  name;
    size_t       name_len;
    const char** tparams;
    Type**       pkinds;
    size_t       pcount;
};
static struct GenericSig* gsig_find(const char* name, size_t len);
static Type* parse_type_ex(bool allow_array);
static Type* parse_type(void);
static ASTNode* new_node(ASTNodeType type);

static Token s_curr;
static jmp_buf s_err_buf;
static SymbolTable* s_symtable;

// Type-parameter names currently in scope (set while parsing a generic function's
// signature/body). parse_type resolves a bare identifier matching one of these to a
// TYPE_PARAM placeholder instead of requiring a registered struct.
static const char** s_type_params = NULL;
static Type**       s_param_kinds  = NULL;   // parallel to s_type_params; NULL entry = type param
static size_t s_type_param_count = 0;

// ─── type aliases ────────────────────────────────────────────────────────────
// `alias Name = <type>` or `alias Name[P, ...] = <type>`. Purely a parse-time
// expansion: the alias body is a Type* whose parameter holes are TYPE_PARAMs,
// and a use-site `Name[args]` resolves by Type_Substitute -- the SAME engine
// generic structs already use. No AST node, no new TYPE_ kind; downstream sees
// only the expanded concrete type. Non-generic aliases (param_count==0) return
// a copy of the body directly.
typedef struct {
    char*  name;
    size_t name_len;
    const char** params;   // TYPE_PARAM hole names (NULL if non-generic)
    Type**       kinds;    // per-param: NULL = type param, non-NULL = value-param pin
    size_t param_count;
    Type*  body;           // body type with TYPE_PARAM (and const-value) holes
} AliasDef;
static AliasDef* s_aliases = NULL;
static size_t s_alias_count = 0, s_alias_cap = 0;

// Scan BACKWARD. The table is append-ordered, so the last matching entry is the
// innermost binding -- shadowing falls out of the scan direction alone, with no depth
// field and no per-scope table. An inner `alias VT = ...` simply sits later in the
// array than an outer one of the same name, and wins.
static AliasDef* alias_lookup(const char* name, size_t len) {
    for (size_t i = s_alias_count; i-- > 0; )
        if (s_aliases[i].name_len == len && strncmp(s_aliases[i].name, name, len) == 0)
            return &s_aliases[i];
    return NULL;
}

// While parsing the PATTERN of a `match T` arm, an undeclared identifier in type
// position is not an "unknown type" error — it is a fresh WILDCARD: a hole in the
// pattern's shape that reflect_unify (reflections.c) will bind to whatever the
// concrete scrutinee has there. `P*` binds P to the pointee; `E[N]` binds E to the
// element and N to the size. This flag switches parse_type_ex into that mode; it
// is set only for the duration of parsing one arm's pattern and cleared before the
// arm body, so the general "unknown type is an error" behaviour is untouched
// everywhere else. The names of wildcards registered while it's on are collected
// below so the arm body can reference them and so branch selection can substitute.
static bool s_in_match_pattern = false;

// `impl {...}` is pattern-only (TYPE_IMPL is a question about a type, not a type), so it
// may only be PRODUCED where a pattern is being parsed. But "a pattern is allowed here" is
// a DIFFERENT question from "an undeclared identifier here is a wildcard", and the two must
// not share a flag: an alias body wants the first (`alias Freeable = impl { fn free() }`)
// and emphatically NOT the second -- with both on, `alias Buf = u8x` silently binds a
// wildcard and yields an 8-byte mystery type instead of erroring on the typo.
//
// A match arm turns BOTH on. An alias body turns on only this one; its own declared params
// (`alias P[X] = X*`) are already in s_type_params and resolve through the ordinary path,
// so it needs no wildcard behaviour at all.
static bool s_pattern_types_ok = false;
static const char** s_match_wildcards = NULL; // names registered in the CURRENT arm pattern
static bool*        s_match_wc_is_size = NULL; // parallel: true = size/value wildcard (u32-pinned)
static size_t s_match_wildcard_count = 0;
static size_t s_match_wildcard_cap = 0;

// Register a pattern wildcard. `is_size` distinguishes an array-size hole (`N` in
// `E[N]`, a VALUE param pinned to u32) from a bare type hole (`P`, `E`, a TYPE
// param). The distinction matters in the arm body: a size wildcard used as a value
// (`(i32)N`) must resolve as a pinned value, so it's published into the type-param
// scope with a u32 pin rather than as a bare (kind-NULL) type param.
static const char* register_match_wildcard_kind(const char* start, size_t len, bool is_size) {
    // Stable, NUL-terminated copy of the identifier text (the lexer token points
    // into the source buffer and isn't NUL-terminated). This pointer is what the
    // TYPE_PARAM / size_param carries and what reflect_unify binds against.
    char* name = (char*)malloc(len + 1);
    memcpy(name, start, len);
    name[len] = '\0';
    if (s_match_wildcard_count >= s_match_wildcard_cap) {
        s_match_wildcard_cap = s_match_wildcard_cap ? s_match_wildcard_cap * 2 : 4;
        s_match_wildcards = (const char**)realloc(s_match_wildcards,
                                                  s_match_wildcard_cap * sizeof(char*));
        s_match_wc_is_size = (bool*)realloc(s_match_wc_is_size,
                                            s_match_wildcard_cap * sizeof(bool));
    }
    s_match_wc_is_size[s_match_wildcard_count] = is_size;
    s_match_wildcards[s_match_wildcard_count++] = name;
    return name;
}
static const char* register_match_wildcard(const char* start, size_t len) {
    return register_match_wildcard_kind(start, len, false);
}

// Is `name` already a wildcard registered in the current arm pattern? A repeated
// occurrence (`Pair[E, E]`, `E*[E]`) must resolve to the SAME TYPE_PARAM, not a
// new one, so the second use compares against the first's binding (non-linear
// pattern = "these two positions are the same type"). Returns the stable name
// pointer if found, NULL otherwise.
static const char* match_wildcard_lookup(const char* start, size_t len) {
    for (size_t i = 0; i < s_match_wildcard_count; i++) {
        if (strlen(s_match_wildcards[i]) == len &&
            strncmp(s_match_wildcards[i], start, len) == 0)
            return s_match_wildcards[i];
    }
    return NULL;
}

// Kind of an in-scope generic param, by name. Returns:
//   -1  not a generic param in scope
//    0  TYPE param  (bare `T`)
//    1  VALUE param (`u32 N`) — and *pin_out receives its pinned type
static int param_kind_lookup(const char* name, size_t len, Type** pin_out) {
    for (size_t i = 0; i < s_type_param_count; i++) {
        if (strlen(s_type_params[i]) == len &&
            strncmp(s_type_params[i], name, len) == 0) {
            Type* pin = s_param_kinds ? s_param_kinds[i] : NULL;
            if (pin_out) *pin_out = pin;
            return pin ? 1 : 0;
        }
    }
    return -1;
}

static Type* type_param_lookup(const char* name, size_t len) {
    Type* pin = NULL;
    int k = param_kind_lookup(name, len, &pin);
    if (k != 0) return NULL;   // not in scope, or a VALUE param -> not a type
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_PARAM;
    // stable name pointer from the scope array
    for (size_t i = 0; i < s_type_param_count; i++) {
        if (strlen(s_type_params[i]) == len && strncmp(s_type_params[i], name, len) == 0) {
            t->param_name = s_type_params[i]; break;
        }
    }
    return t;
}

static bool expr_mentions_generic_param(ASTNode* n); // fwd decl: mutually recursive with type_mentions_generic_param below

// Does this Type reference any generic param currently in scope, anywhere in its
// structure (pointer base, array element/count_expr, a const-value's pin/defer)?
// Sibling of expr_mentions_generic_param for the Type domain — needed because some
// parse-time constructs (sizeof(type)) resolve a Type rather than an ASTNode, and a
// bare identifier used as a type name is stamped TYPE_PARAM with no ASTNode at all
// to walk. Same fail-safe convention: unrecognized shapes return true (defer),
// never false, so a new TypeClass added later can't silently reintroduce this bug.
static bool type_mentions_generic_param(Type* t) {
    if (!t) return false;
    switch (t->cls) {
        case TYPE_PARAM:
            return true; // a bare `T`/`N`-shaped type IS a generic param, by construction
        case TYPE_POINTER:
            return type_mentions_generic_param(t->pointer_base);
        case TYPE_ARRAY:
            return type_mentions_generic_param(t->array.element) ||
                   (t->array.count_expr && expr_mentions_generic_param(t->array.count_expr));
        case TYPE_CONST_VALUE:
            return (t->cval.defer && expr_mentions_generic_param(t->cval.defer)) ||
                   type_mentions_generic_param(t->cval.pin);
        case TYPE_PRIMITIVE:
            return false; // no substructure to reference a param through
        case TYPE_STRUCT:
        case TYPE_FUNCTION:
        default:
            // A generic struct instantiation's own type-args, or a function type's
            // param/return types, could reference an outer param through paths this
            // switch doesn't walk yet — fail safe rather than risk the sizeof(N)/
            // sizeof(u32[N]) class of bug recurring for a shape not covered above.
            return true;
    }
}

// Does this expression reference any generic param currently in scope? Such an
// expression must NOT be eagerly const-folded/typechecked at parse time — a
// same-named global const would otherwise capture it (e.g. `T[N]` inside
// `struct A[T, u32 N]` while a global `const N` exists), or the param's symbol
// simply has no concrete type yet to infer against. It defers to Type_Substitute,
// which folds it under the generic frame where the param shadows the global
// (correct pinning precedence).
//
// Fail-safe by construction: any node type this switch doesn't explicitly know to
// be param-free returns true (assume it might reference a param, defer). This is
// deliberate — every parse-time-fold bug found so far (sizeof(N), sizeof(u32[N]),
// match's scrutinee, match arm patterns) was exactly this function silently
// returning false for a node shape it hadn't been taught about yet. Getting this
// wrong in the "defer too often" direction just means a value resolves slightly
// later (at instantiation) instead of at parse time — harmless. Getting it wrong
// in the "defer too rarely" direction produces a wrong answer or a bogus parse
// error. Only whitelist a case here once you've confirmed it truly cannot
// contain a generic-param reference in any of its sub-fields.
static bool expr_mentions_generic_param(ASTNode* n) {
    if (!n) return false;
    switch (n->type) {
        case AST_IDENT:
            for (size_t i = 0; i < s_type_param_count; i++)
                if (strlen(s_type_params[i]) == n->ident.name_len &&
                    strncmp(s_type_params[i], n->ident.name, n->ident.name_len) == 0)
                    return true;
            for (size_t i = 0; i < n->ident.type_arg_count; i++)
                if (type_mentions_generic_param(n->ident.type_args[i]))
                    return true;
            return false;
        case AST_INT_LITERAL:
        case AST_STRING:
            return false; // literals are self-contained, no sub-expressions to walk
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
        case AST_SHL: case AST_SHR:
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR:
            return expr_mentions_generic_param(n->binary.left) ||
                   expr_mentions_generic_param(n->binary.right);
        case AST_LOGICAL_NOT: case AST_BIT_NOT:
        case AST_DEREF: case AST_ADDR:
            return expr_mentions_generic_param(n->unary);
        case AST_CAST:
            return expr_mentions_generic_param(n->cast.expr);
        case AST_SIZEOF:
            return type_mentions_generic_param(n->sizeof_expr.type) ||
                   (n->sizeof_expr.defer_expr && expr_mentions_generic_param(n->sizeof_expr.defer_expr));
        case AST_TYPE_EXPR:
            // Same field, same shape as AST_SIZEOF — a bare type-param
            // reference (T == i32, etc.) mentions a generic param exactly
            // when its wrapped Type* does.
            return type_mentions_generic_param(n->sizeof_expr.type);
        case AST_ALIGNOF:
            return type_mentions_generic_param(n->sizeof_expr.type);
        case AST_OFFSETOF:
        case AST_NAMEOF:
            // Only reached un-folded (see parse_primary) when the struct type
            // is still a generic param or the index references one -- so this
            // is always true in practice, but walk both fields for real
            // rather than assuming, same discipline as every other case here.
            return type_mentions_generic_param(n->field_ref_expr.type) ||
                   (n->field_ref_expr.index_expr && expr_mentions_generic_param(n->field_ref_expr.index_expr));
        case AST_FIELD:
            return expr_mentions_generic_param(n->field.base);
        case AST_INDEX:
            return expr_mentions_generic_param(n->index.base) ||
                   expr_mentions_generic_param(n->index.index);
        case AST_CALL:
            // A call itself isn't a generic-param reference, but its arguments
            // might be (e.g. `helper(N)` inside a generic body). Previously this
            // fell into the `default: return true` fail-safe below, which meant
            // EVERY call — including a fully-constant one like `width(3)` with no
            // generic params anywhere in scope — was unconditionally deferred as
            // an array-size expression. Deferred count_expr is ONLY ever
            // re-resolved by Type_Substitute during generic instantiation; for a
            // non-generic type there is no such second pass, so the array size
            // silently stuck at 0 forever with no error. Recursing into the args
            // (and target_expr, for `obj.method(...)`) lets a genuinely constant
            // call go straight to ConstEval instead.
            if (n->call.target_expr && expr_mentions_generic_param(n->call.target_expr))
                return true;
            for (size_t i = 0; i < n->call.type_arg_count; i++)
                if (type_mentions_generic_param(n->call.type_args[i]))
                    return true;
            for (size_t i = 0; i < n->call.arg_count; i++)
                if (expr_mentions_generic_param(n->call.args[i]))
                    return true;
            return false;
        default:
            return true; // fail-safe: unrecognized shape, assume it may reference a param
    }
}

// Parse a generic parameter list `[ ... ]` (opening bracket is current token).
// Each param is either:
//   * a TYPE param  — a bare identifier not preceded by a type: `T`
//   * a VALUE param — a type followed by a name: `u32 N`, `Point P`, `List[u32] Xs`
// The pin type of a value param may reference type params declared EARLIER in the
// same list (e.g. `[T, T scale]`), so names are installed into the live scope
// (s_type_params/s_param_kinds) incrementally as we parse. On return, the scope
// arrays point at the freshly built list; the caller is responsible for saving
// and restoring the previous scope around this call.
static void parse_generic_param_list_impl(const char** inherited_names, Type** inherited_kinds,
                                          size_t inherited_count,
                                          const char*** names_out, Type*** kinds_out,
                                          size_t* count_out) {
    advance(); // consume '['
    size_t cap = inherited_count + 4;
    size_t count = inherited_count;
    const char** names = (const char**)malloc(cap * sizeof(char*));
    Type**       kinds = (Type**)malloc(cap * sizeof(Type*));

    if (inherited_count) {
        memcpy(names, inherited_names, inherited_count * sizeof(char*));
        memcpy(kinds, inherited_kinds, inherited_count * sizeof(Type*));
    }

    // Install incrementally so a later pin can see earlier params.
    s_type_params = names;
    s_param_kinds = kinds;
    s_type_param_count = count;

    while (s_curr.type != TOK_RBRACKET && s_curr.type != TOK_EOF) {
        Type* pin = NULL;
        // A VALUE param begins with a type (primitive keyword, or a name that
        // resolves to a struct/generic/earlier-type-param). A bare identifier that
        // is NOT itself a type start is a TYPE param being declared.
        if (curr_begins_type()) {
            pin = parse_type();
            if (!pin) parse_error("Expected type in generic value-parameter");
        }
        if (s_curr.type != TOK_IDENTIFIER)
            parse_error(pin ? "Expected name after type in generic value-parameter"
                            : "Expected generic parameter name");
        if (count >= cap) {
            cap *= 2;
            names = realloc(names, cap * sizeof(char*));
            kinds = realloc(kinds, cap * sizeof(Type*));
            s_type_params = names; s_param_kinds = kinds; // keep scope pointers live
        }
        names[count] = strndup(s_curr.start, s_curr.length);
        kinds[count] = pin;      // NULL => type param; non-NULL => value param pin
        count++;
        s_type_param_count = count;   // publish this param before parsing the next pin
        advance();
        if (s_curr.type == TOK_COMMA) advance();
        else break;
    }
    if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after generic parameters");
    advance();
    *names_out = names; *kinds_out = kinds; *count_out = count;
}

static void parse_generic_param_list(const char*** names_out, Type*** kinds_out, size_t* count_out) {
    parse_generic_param_list_impl(NULL, NULL, 0, names_out, kinds_out, count_out);
}

static void parse_generic_param_list_with_prefix(const char** inherited_names, Type** inherited_kinds,
                                                size_t inherited_count,
                                                const char*** names_out, Type*** kinds_out,
                                                size_t* count_out) {
    parse_generic_param_list_impl(inherited_names, inherited_kinds, inherited_count,
                                  names_out, kinds_out, count_out);
}

// Parse ONE generic value-argument expression and fold it (via the constexpr
// engine) into a TYPE_CONST_VALUE pinned to `pin`. Scalars carry their int64
// payload; aggregates (struct/array/enum pins) are materialized into the
// persistent comptime arena and carried as an offset. `pin` is always
// non-NULL here — the caller only reaches this function for a struct's
// declared value-generic slot, which is always kinded (e.g. `[T, u32 N]`).
static Type* parse_generic_value_arg(Type* pin) {
    // In a `match T` pattern, a bare undeclared identifier in a VALUE slot (e.g.
    // `N` in `Stack[E, N]`) is a value wildcard: don't try to const-fold it, mark
    // it as a hole to be bound by reflect_unify. We record it as a TYPE_CONST_VALUE
    // whose `defer` is an AST_IDENT naming the wildcard — unify recognizes that
    // shape and binds N to the concrete arg's value.
    if (s_in_match_pattern && s_curr.type == TOK_IDENTIFIER) {
        // Only treat as a wildcard if it's not already a resolvable constant/param.
        Type* dummy;
        bool is_known = (param_kind_lookup(s_curr.start, s_curr.length, &dummy) >= 0);
        if (!is_known) {
            Symbol* sym = SymTable_Find(s_symtable, s_curr.start, s_curr.length);
            if (sym && (sym->kind == SYM_CONST || sym->kind == SYM_GLOBAL)) is_known = true;
        }
        // Peek: a bare identifier immediately followed by `,` or `]` is a lone name,
        // i.e. a candidate wildcard (not part of a larger constant expression).
        if (!is_known) {
            LexerState save; Lexer_Save(&save);
            Token idtok = s_curr;
            Token nxt = Lexer_NextToken();
            Lexer_Restore(&save);
            if (nxt.type == TOK_COMMA || nxt.type == TOK_RBRACKET) {
                const char* wname = match_wildcard_lookup(idtok.start, idtok.length);
                if (!wname) wname = register_match_wildcard_kind(idtok.start, idtok.length, true);
                advance(); // consume the identifier
                Type* v = make_const_value_type(pin);
                // Stash the wildcard name via a synthetic AST_IDENT in `defer`.
                ASTNode* nameref = make_ident_node(wname, strlen(wname), NULL);
                v->cval.defer = nameref;
                return v;
            }
        }
    }

    ASTNode* expr = parse_expr_prec(0);
    Type* v = make_const_value_type(pin);

    bool aggregate = pin && (pin->cls == TYPE_STRUCT ||
                             (pin->cls == TYPE_ARRAY));
    if (aggregate) {
        // resolve any bare {..} literal against the pin, then persist its bytes
        resolve_brace_literal(expr, pin);
        int64_t off = ConstEval_AggPersist(expr, pin);
        if (off < 0) {
            // References an outer generic param — defer to instantiation time.
            v->cval.defer = expr;
        } else {
            v->cval.is_agg = true;
            v->cval.agg_off = (uint32_t)off;
        }
    } else {
        int64_t val;
        // expr_mentions_generic_param is a FAIL-SAFE, not a precise "is this
        // actually unresolvable right now" check: TYPE_STRUCT/TYPE_FUNCTION
        // type-args always answer true (see its own comment), even when the
        // struct/function they name is fully concrete (e.g. `t_area[Circle]`,
        // a generic FUNCTION reference used as a const-generic ARGUMENT --
        // `Circle` looks param-shaped to that switch, but neither it nor
        // `t_area` actually references anything in an outer generic frame).
        // Deferring is only meaningful if there IS an outer frame to resolve
        // it later (s_type_param_count > 0 -- we're parsing this expression
        // while some enclosing generic's params are in scope, e.g. a value-arg
        // written inside a generic template body). With no generic scope
        // active at all, deferring is provably pointless: no future
        // Type_Substitute call will ever revisit this expression, so it would
        // sit as cval.defer forever and read back as a silent scalar 0 the
        // first time something (e.g. clone_ast monomorphizing a method body
        // that reads this param) assumes it's already resolved. Try the real
        // fold immediately instead in that case.
        // `t_area[Circle]` (a generic function referenced with EXPLICIT type
        // args, no call parens -- see the AST_IDENT construction site above)
        // still names the raw TEMPLATE symbol at this point; typecheck
        // normally instantiates it later (Typecheck_Tree's AST_IDENT case).
        // A const-generic argument needs the concrete value NOW, so do that
        // instantiation eagerly here -- same call, just earlier -- and
        // rewrite the node to the real instantiation symbol before folding.
        // Without this, ConstEval correctly refuses to fold a still-generic
        // symbol (ce_eval_ident's own generic_decl guard), the value falls
        // through as unresolved, and nothing downstream (clone_ast cloning
        // this struct's methods, none of which are themselves generic) ever
        // revisits it -- so it silently reads back as a scalar 0.
        if (expr->type == AST_IDENT && expr->ident.sym && expr->ident.sym->generic_decl &&
            expr->ident.type_arg_count > 0) {
            expr->ident.sym = Generic_Instantiate(expr->ident.sym, expr->ident.type_args,
                                                  expr->ident.type_arg_count);
        }
        bool maybe_outer_param = expr_mentions_generic_param(expr) && s_type_param_count > 0;
        if (maybe_outer_param) {
            // Depends on an outer, not-yet-concrete generic param (e.g. this
            // struct is itself being defined/used inside another generic's
            // body) — legitimate deferral. Type_Substitute re-folds once the
            // outer param is concrete.
            v->cval.defer = expr;
        } else if (ConstEval(expr, &val)) {
            v->cval.is_agg = false;
            v->cval.scalar = val;
        } else {
            // Doesn't reference any generic param, so no future instantiation
            // will ever make this foldable either — it's just a genuine
            // runtime value (e.g. a plain non-const local). Previously this
            // fell into the same `defer` path as the legitimate case above,
            // which then silently sat unresolved forever and read back as a
            // scalar 0 downstream (e.g. `Foo[k]` with plain `u32 k` silently
            // became `Foo[0]`, with no error, and no bounds checking on the
            // resulting zero-sized array). Hard error now instead.
            parse_error("generic value argument must be a compile-time constant");
        }
    }
    return v;
}

// Parses a generic argument LIST (the comma-separated contents between an
// already-consumed `[` and a not-yet-consumed `]`) shared by every use site
// that instantiates a generic with explicit arguments -- a struct type
// (`Container[i32, {.val=5}]`), an explicit generic function call
// (`make[i32, {.val=5}]()`), and a generic alias (`SomeAlias[i32, {.val=5}]`).
// These were three independently hand-written copies of the same loop before
// this was pulled out; two of the three were missing the SAME fix (below) at
// the same time, found by testing one, fixing it, then discovering the
// second call site still had the bug -- exactly the "same primitive needed
// at multiple sites, not actually shared" pattern this whole session kept
// finding elsewhere. One function now; a fix here reaches every caller.
//
// `param_names`/`param_kinds`/`param_count` describe the generic's OWN
// declared parameter list (NULL kind = type param, non-NULL = value param
// pinned to that type). Returns the parsed args via `*out_targs`/`*out_tcount`
// (heap-allocated, caller-owned) and leaves s_curr on the `]` (or whatever
// stopped the loop) -- the caller still does its own `]`-expect/advance and
// arity check, since the three call sites want different error wording there.
static void parse_generic_arg_list(const char** param_names, Type** param_kinds, size_t param_count,
                                    Type*** out_targs, size_t* out_tcount) {
    size_t tcap = param_count ? param_count : 4;
    Type** targs = (Type**)malloc(tcap * sizeof(Type*));
    size_t tcount = 0;
    while (s_curr.type != TOK_RBRACKET && s_curr.type != TOK_EOF) {
        Type* pin = (param_kinds && tcount < param_count) ? param_kinds[tcount] : NULL;
        // A later param's pin type may itself reference an EARLIER param in the
        // same list (`Container[T, Box[T] val]` -- the second param's pin is
        // `Box[T]`, naming the first). Substitute using the args already parsed
        // so far before resolving this one, so an inline brace literal
        // (`Container[i32, {.val=5}]`) resolves `.val` against the REAL,
        // concrete `Box[i32]`, not the still-abstract declared `Box[T]`.
        if (pin && tcount > 0 && param_names) {
            pin = Type_Substitute(pin, param_names, targs, tcount);
        }
        Type* ta;
        if (pin) {
            ta = parse_generic_value_arg(pin);
        } else {
            ta = parse_type();
            if (!ta) parse_error("Expected a type argument here (a bare value "
                                  "argument requires the parameter to be declared "
                                  "with a pinned type, e.g. `[T, u32 N]`, not `[T, N]`)");
        }
        if (tcount >= tcap) { tcap *= 2; targs = realloc(targs, tcap * sizeof(Type*)); }
        targs[tcount++] = ta;
        if (s_curr.type == TOK_COMMA) advance();
        else break;
    }
    *out_targs = targs;
    *out_tcount = tcount;
}

// True iff a newline appeared between the previous token and s_curr. Captured
// from the lexer at scan time and stable until the next advance(). Used to stop
// the postfix chain from gluing a `(`/`[` at the start of a NEW statement onto
// the value that ended the previous one (Torrent is newline-terminated, no `;`):
//   arr[0] = mk          <- statement ends with a value
//   (fn(u32) u32)[2]* p  <- next statement; the `(` must NOT read as `mk(...)`
static bool s_curr_newline_before = false;

// Full parser position snapshot: the lexer's scan cursor (LexerState) PLUS the
// already-lexed lookahead token (s_curr) and its newline flag. A LexerState
// alone only rewinds the SCANNER -- it says nothing about the token already
// sitting in s_curr, which is stale the moment the scanner moves. Every
// speculative-parse site in this file used to snapshot/restore these three
// fields by hand (14 call sites, no two spelled quite the same way); this is
// the one shared checkpoint for all of them.
typedef struct {
    LexerState lex;
    Token curr;
    bool curr_newline_before;
} ParseCheckpoint;
static void parser_save(ParseCheckpoint* cp) {
    Lexer_Save(&cp->lex);
    cp->curr = s_curr;
    cp->curr_newline_before = s_curr_newline_before;
}
static void parser_restore(const ParseCheckpoint* cp) {
    Lexer_Restore(&cp->lex);
    s_curr = cp->curr;
    s_curr_newline_before = cp->curr_newline_before;
}

// `with` desugaring: after replaying N prefix tokens, switch the lexer back to
// the body source. s_with_switch_after counts how many more advance() calls
// before the switch fires. On fire: restore s_with_switch_st, set s_curr to
// s_with_switch_tok (the entry's first token), and continue from there.
static bool       s_with_switch_active = false;
static int        s_with_switch_after  = 0;
static LexerState s_with_switch_st;
static Token      s_with_switch_tok;
static bool       s_with_switch_nl;
// Set while parsing field values inside a named struct literal `{.f=expr .g=expr}`.
// When set, a leading `.` after a newline is treated as the start of the next field
// (not a postfix field-access on the previous value), matching method-chain intent.
static bool s_in_struct_literal_field = false;

// Set only while parsing the type after `new`: a postfix `*`/`[` that begins a
// NEW LINE belongs to the next statement, not this type. `new u32` <nl> `*x = 42`
// must allocate a u32, not swallow the `*x` deref into `new (u32*)`. (Torrent is
// newline-terminated; this is the newline playing C++'s post-`new` `;` role.)
// Off everywhere else, so ordinary type positions like `u32*[4]` are unaffected.
static bool s_new_type_no_nl_postfix = false;
static int s_loop_depth = 0;   // >0 when inside a while/for body

static void advance(void) {
    if (s_with_switch_active) {
        if (s_with_switch_after == 0) {
            // Fire: switch lexer to body source, yield entry's first token.
            Lexer_Restore(&s_with_switch_st);
            s_curr                = s_with_switch_tok;
            s_curr_newline_before = s_with_switch_nl;
            s_with_switch_active  = false;
            return;
        }
        s_with_switch_after--;
    }
    s_curr = Lexer_NextToken();
    s_curr_newline_before = Lexer_NewlineBefore;
}


static void parse_error(const char* msg) {
    // Delegates to the shared error.c formatter (Error_AtToken) instead of
    // rolling its own fprintf, but keeps exactly the same behavior: print,
    // then longjmp back to whichever setjmp is active (Parse_Signatures'
    // pre-pass, or Parse_Block's real pass -- see compiler.h's note on
    // Error_AtToken for why parser.c can't just exit() here).
    //
    // NOTE: Parse_Signatures' pre-pass setjmp comment says it bails out
    // "quietly rather than double-reporting" -- that was already not fully
    // true before this change (a malformed generic header can print once
    // from the pre-pass and once from the real pass; verified independent
    // of this refactor). Preserved as-is here rather than silently patched,
    // since suppressing it is a separate, deliberate fix, not a
    // side-effect of sharing the printer.
    Error_AtToken(s_curr, msg, &s_err_buf);
}

static ASTNode* new_node(ASTNodeType type) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
    // DEMO: stamp the location of whatever token is current when this node
    // is built. Good enough for the single caret-error demo site; a couple
    // of nodes get created after their "natural" token has already been
    // consumed (e.g. struct literals build their sdef-carrying node before
    // reading fields) so this is an approximation, not exact per-node-kind
    // placement -- fine for proving the mechanism, not yet audited node-by-node.
    node->line = s_curr.line;
    node->column = s_curr.column;
    node->filename = s_curr.filename;
    return node;
}

static ASTNode* make_ident_node(const char* name, size_t len, Symbol* sym) {
    ASTNode* node = new_node(AST_IDENT);
    node->ident.name = name;
    node->ident.name_len = len;
    node->ident.sym = sym;
    return node;
}

static ASTNode* make_int_literal(uint64_t value, int lit_kind) {
    ASTNode* node = new_node(AST_INT_LITERAL);
    node->lit_kind = lit_kind;
    node->int_value = value;
    return node;
}

// `TYPE name = init_expr` as sugar over `TYPE name` (bare, zero-init)
// immediately followed by a REAL `name = init_expr` AST_ASSIGN, wrapped in a
// `transparent` AST_BLOCK (not a real scope -- see the for-loop init wrapper
// and parse_unpack for the same flag/reasoning: `name` must stay visible to
// whatever follows in the REAL enclosing block, and ConstEval must not unwind
// its comptime binding when this synthetic wrapper ends).
//
// STAGED ROLLOUT: only parse_decl_or_expr_statement (ordinary `TYPE name =
// expr` statements) uses this so far. for-loops, unpack, match-arm bindings,
// and generic-const declarations still build AST_DECLARATION with init_expr
// set directly, and AST_DECLARATION's own typecheck/codegen still handles
// that shape unchanged -- migrating this incrementally, one caller at a time
// with a full test run after each, rather than deleting the old path before
// every caller is confirmed moved off it.
static ASTNode* make_decl_stmt(Type* var_type, const char* name, size_t name_len,
                                Symbol* sym, ASTNode* init_expr) {
    ASTNode* decl = new_node(AST_DECLARATION);
    decl->decl.var_type = var_type;
    decl->decl.name = name;
    decl->decl.name_len = name_len;
    decl->decl.sym = sym;
    if (!init_expr) return decl;

    ASTNode* assign = new_node(AST_ASSIGN);
    assign->binary.left = make_ident_node(name, name_len, sym);
    assign->binary.right = init_expr;

    ASTNode* blk = new_node(AST_BLOCK);
    blk->block.capacity = 2;
    blk->block.count = 2;
    blk->block.statements = (ASTNode**)malloc(2 * sizeof(ASTNode*));
    blk->block.transparent = true;
    blk->block.statements[0] = decl;
    blk->block.statements[1] = assign;
    return blk;
}

static Type* make_pointer_type(Type* base) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_POINTER;
    t->pointer_base = base;
    return t;
}

static Type* make_array_type(Type* elem, uint64_t count, ASTNode* count_expr, const char* size_param) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_ARRAY;
    t->array.element = elem;
    t->array.count = count;
    t->array.count_expr = count_expr;
    t->array.size_param = size_param;
    return t;
}

static Type* make_const_value_type(Type* pin) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_CONST_VALUE;
    t->cval.pin = pin;
    return t;
}

static ASTNode* make_block(void) {
    ASTNode* block = new_node(AST_BLOCK);
    block->block.capacity = 16;
    block->block.statements = (ASTNode**)malloc(block->block.capacity * sizeof(ASTNode*));
    block->block.count = 0;
    return block;
}

static void append_block_statement(ASTNode* block, ASTNode* stmt) {
    if (block->block.count >= block->block.capacity) {
        block->block.capacity *= 2;
        block->block.statements = (ASTNode**)realloc(block->block.statements,
                                                      block->block.capacity * sizeof(ASTNode*));
    }
    block->block.statements[block->block.count++] = stmt;
}

static void parse_braced_statements(ASTNode* block, bool pop_aliases) {
    size_t alias_mark = pop_aliases ? s_alias_count : 0;
    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        ASTNode* stmt = parse_statement();
        if (stmt) append_block_statement(block, stmt);
    }

    if (s_curr.type != TOK_RBRACE) {
        parse_error("Expected '}' to end block");
    }
    advance();
    if (pop_aliases) s_alias_count = alias_mark;
}

static ASTNode* parse_braced_block(bool create_scope, bool pop_aliases) {
    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' to start block");
    advance();

    SymbolTable* prev_table = s_symtable;
    if (create_scope) {
        s_symtable = SymTable_Create(prev_table);
        s_symtable->is_function_scope = prev_table->is_function_scope;
    }

    ASTNode* block = make_block();
    parse_braced_statements(block, pop_aliases);

    if (create_scope) s_symtable = prev_table;
    return block;
}

static ASTNode* parse_expr_prec(int min_prec); // used by parse_type for array dims

// Struct_Find needs a null-terminated name, but every caller here only has a
// (start, length) pair from a token — this was duplicated three times as an
// inline `char tmp[256]; memcpy; tmp[n]='\0'; Struct_Find(tmp)`, each with
// its own independent 256-char truncation limit. One shared helper instead.
static StructDef* struct_find_by_token_text(const char* text, size_t len) {
    char tmp[256];
    size_t n = len < sizeof(tmp) - 1 ? len : sizeof(tmp) - 1;
    memcpy(tmp, text, n);
    tmp[n] = '\0';
    return Struct_Find(tmp);
}


static bool parse_impl_pattern_type(Type* base_t) {
    if (!s_pattern_types_ok) return false;
    advance(); // 'impl'
    if (s_curr.type != TOK_LBRACE) return false;
    advance(); // '{'

    const char** mnames = NULL; size_t* mlens = NULL; Type** msigs = NULL;
    size_t mcount = 0, mcap = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        if (s_curr.type != TOK_FN) parse_error("Expected 'fn' inside an `impl {...}` pattern");
        advance(); // 'fn'
        if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected a method name in an `impl {...}` pattern");
        Token mname = s_curr;
        advance();

        if (s_curr.type != TOK_LPAREN) parse_error("Expected '(' after the method name in an `impl {...}` pattern");
        advance();
        Type** ptypes = NULL; size_t pcount = 0, pcap = 0;
        while (s_curr.type != TOK_RPAREN && s_curr.type != TOK_EOF) {
            Type* pt = parse_type();
            if (!pt) parse_error("Expected a parameter type in an `impl {...}` pattern");
            if (pcount >= pcap) { pcap = pcap ? pcap * 2 : 4; ptypes = realloc(ptypes, pcap * sizeof(Type*)); }
            ptypes[pcount++] = pt;
            if (s_curr.type == TOK_COMMA) advance(); else break;
        }
        if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' in an `impl {...}` pattern");
        advance();

        Type* ret = NULL;
        if (s_curr.type != TOK_RBRACE && !Lexer_NewlineBefore && curr_begins_type())
            ret = parse_type_ex(true);

        Type* sig = (Type*)calloc(1, sizeof(Type));
        sig->cls = TYPE_FUNCTION;
        sig->function.return_type = ret;
        sig->function.param_types = ptypes;
        sig->function.param_count = pcount;
        sig->function.is_vararg = false;

        if (mcount >= mcap) {
            mcap = mcap ? mcap * 2 : 4;
            mnames = (const char**)realloc(mnames, mcap * sizeof(char*));
            mlens  = (size_t*)realloc(mlens,  mcap * sizeof(size_t));
            msigs  = (Type**)realloc(msigs,  mcap * sizeof(Type*));
        }
        mnames[mcount] = mname.start;
        mlens[mcount]  = mname.length;
        msigs[mcount]  = sig;
        mcount++;

        if (s_curr.type == TOK_SEMI || s_curr.type == TOK_COMMA) advance();
    }

    if (mcount == 0) parse_error("Expected at least one `fn` inside an `impl {...}` pattern");
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to close an `impl {...}` pattern");
    advance();

    base_t->cls = TYPE_IMPL;
    base_t->impl_pat.method_names     = mnames;
    base_t->impl_pat.method_name_lens = mlens;
    base_t->impl_pat.sigs             = msigs;
    base_t->impl_pat.method_count     = mcount;
    return true;
}

static bool parse_anonymous_aggregate_type(Type* base_t) {
    bool anon_is_enum    = (s_curr.type == TOK_ENUM);
    bool anon_is_overlap = (s_curr.type == TOK_UNION);
    const char* anon_kw  = anon_is_enum ? "enum" : (anon_is_overlap ? "union" : "struct");
    advance(); // 'struct' / 'union' / 'enum'
    if (s_curr.type != TOK_LBRACE) return false;
    advance(); // '{'

    Type* ftypes[64];
    const char* fnames[64];
    size_t fcount = 0;
    int pack_idx = -1;
    
    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        Type* ft = NULL;
        if (anon_is_enum) {
            if (curr_begins_type()) {
                LexerState save;
                Lexer_Save(&save);
                Token   at_start = s_curr;
                Type*   spec = parse_type();
                // A real payload-type spec is followed by either the variant's own
                // name (`i32 Circle`) or, for a pack-tail wildcard, `...` before the
                // tail's binding name (`Rest... r`) -- without the second case, `Rest`
                // parsed fine as a type but the lookahead saw `...` next (not an
                // identifier) and wrongly concluded "not a payload type after all,
                // this must be a bare no-payload variant literally named Rest",
                // silently eating the wildcard as a variant name instead of a type.
                if (spec && (s_curr.type == TOK_IDENTIFIER || s_curr.type == TOK_ELLIPSIS)) {
                    ft = spec;
                } else {
                    Lexer_Restore(&save);
                    s_curr = at_start;
                    ft = NULL;
                }
            }
        } else {
            ft = parse_type();
            if (!ft) parse_error("Expected field type in anonymous aggregate");
        }
        
        if (s_curr.type == TOK_ELLIPSIS) {
            if (pack_idx != -1)
                parse_error("at most one `T...` pack-tail field is allowed per anonymous aggregate");
            pack_idx = (int)fcount;
            advance();
        }
        
        // A field name is optional. If omitted, synthesize "_N" -- same scheme
        // Struct_MakeAnon already uses internally for call-site pack bundles, so
        // an unnamed hand-written field and a pack-expanded one land on the same
        // name. If GIVEN, it's kept and fully meaningful (ordinary anon structs
        // are used as real value types with real `.field` access all over the
        // codebase -- e.g. a function returning `struct { i32 q  i32 r }` and
        // callers doing `.q`/`.r` -- so an explicit name must never be discarded).
        // enum payload variants are different: the variant name IS identity, so
        // it stays required there.
        if (anon_is_enum) {
            if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected variant name in anonymous enum");
        }
        if (fcount >= 64) parse_error("too many fields in anonymous aggregate");
        ftypes[fcount] = ft;
        if (s_curr.type == TOK_IDENTIFIER) {
            fnames[fcount] = strndup(s_curr.start, s_curr.length);
            advance(); // field/variant name
        } else {
            char fnbuf[16];
            snprintf(fnbuf, sizeof(fnbuf), "_%zu", fcount);
            fnames[fcount] = strdup(fnbuf);
        }
        fcount++;

        if (s_curr.type == TOK_EQ) {
            parse_error("anonymous struct fields cannot have defaults "
                        "(anonymous struct identity is its field types; use a named struct for defaults)");
        }
        if (s_curr.type == TOK_SEMI) advance();
        else if (pack_idx == (int)(fcount - 1) && s_curr.type != TOK_RBRACE)
            parse_error("a `T...` pack-tail field must be the last field in the anonymous struct");
    }
    
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end anonymous struct");
    advance();

    char namebuf[1024];
    size_t off = 0;
    off += snprintf(namebuf + off, sizeof(namebuf) - off, "%s{", anon_kw);
    for (size_t i = 0; i < fcount; i++) {
        char tn[128];
        if (ftypes[i]) Type_ToString(ftypes[i], tn, sizeof(tn));
        else           snprintf(tn, sizeof(tn), "#%s", fnames[i]);
        off += snprintf(namebuf + off, sizeof(namebuf) - off, "%s%s%s",
                        (int)i == pack_idx ? "..." : "", tn,
                        (i + 1 < fcount) ? "," : "");
    }
    snprintf(namebuf + off, sizeof(namebuf) - off, "}");

    StructDef* sd = Struct_Register(namebuf, strlen(namebuf));
    if (sd->field_count == 0 && !sd->laid_out) {
        sd->is_enum        = anon_is_enum;
        sd->is_overlapping = anon_is_overlap;
        sd->is_anonymous = true;
        sd->pack_field_index = pack_idx;
        sd->fields = (StructField*)calloc(fcount ? fcount : 1, sizeof(StructField));
        sd->field_count = fcount;
        for (size_t i = 0; i < fcount; i++) {
            sd->fields[i].name = fnames[i];
            sd->fields[i].type = ftypes[i];
            sd->fields[i].offset = 0;
        }
        Struct_Layout(sd);
    } else {
        for (size_t i = 0; i < fcount; i++) free((void*)fnames[i]);
    }

    base_t->cls = TYPE_STRUCT;
    base_t->struct_name = sd->name;
    return true;
}

static void parse_function_type(Type* base_t) {
    advance();
    if (s_curr.type != TOK_LPAREN) parse_error("Expected '(' after 'fn' for function type");
    advance();
    Type** param_types = NULL;
    size_t param_count = 0;
    size_t param_cap = 0;
    if (s_curr.type != TOK_RPAREN) {
        while (1) {
            if (param_count >= param_cap) {
                param_cap = param_cap ? param_cap * 2 : 4;
                param_types = realloc(param_types, param_cap * sizeof(Type*));
            }
            param_types[param_count++] = parse_type();
            if (s_curr.type == TOK_COMMA) advance();
            else break;
        }
    }
    if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' in function type");
    advance();
    Type* ret_type = NULL;
    if (curr_begins_type()) {
        ret_type = parse_type_ex(true);
    }
    base_t->cls = TYPE_FUNCTION;
    base_t->function.return_type = ret_type;
    base_t->function.param_types = param_types;
    base_t->function.param_count = param_count;
}

static Type* parse_alias_or_type_param_type(Type* base_t) {
    Type* tp = type_param_lookup(s_curr.start, s_curr.length);
    if (tp) {
        free(base_t);
        advance();
        return tp;
    }
    
    AliasDef* al = alias_lookup(s_curr.start, s_curr.length);
    if (al) {
        advance();
        Type* expanded;
        if (al->param_count == 0) {
            expanded = al->body;
        } else {
            if (s_curr.type != TOK_LBRACKET)
                parse_error("generic alias used without type arguments");
            advance();
            Type** targs; size_t tcount;
            parse_generic_arg_list(al->params, al->kinds, al->param_count, &targs, &tcount);
            if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after alias type arguments");
            advance();
            if (tcount != al->param_count) parse_error("wrong number of type arguments for alias");
            expanded = Type_Substitute(al->body, al->params, targs, al->param_count);
        }
        free(base_t);
        return expanded;
    }
    
    StructDef* sd = struct_find_by_token_text(s_curr.start, s_curr.length);
    if (!sd) {
        if (s_in_match_pattern) {
            const char* wname = match_wildcard_lookup(s_curr.start, s_curr.length);
            if (!wname) wname = register_match_wildcard(s_curr.start, s_curr.length);
            advance();
            base_t->cls = TYPE_PARAM;
            base_t->param_name = wname;
            return base_t;
        }
        free(base_t);
        return NULL;
    }
    
    advance();
    if (sd->is_generic) {
        if (s_curr.type != TOK_LBRACKET) {
            // No `[...]` -- a bare, deliberately unapplied template (e.g. `M`
            // bound to `Box` in `HKT[Box, i32]`), not an error here.
            base_t->cls = TYPE_STRUCT;
            base_t->struct_name = sd->name;
            base_t->struct_unapplied = true;
            return base_t;
        }
        advance();
        Type** targs; size_t tcount;
        parse_generic_arg_list(sd->type_params, sd->param_kinds, sd->type_param_count, &targs, &tcount);
        if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after generic struct type arguments");
        advance();
        StructDef* inst = Struct_Instantiate(sd, targs, tcount);
        base_t->cls = TYPE_STRUCT;
        base_t->struct_name = inst->name;
        return base_t;
    }
    
    base_t->cls = TYPE_STRUCT;
    base_t->struct_name = sd->name;
    return base_t;
}
static Type* parse_type_ex(bool allow_array) {
    Type* base_t = (Type*)calloc(1, sizeof(Type));
    base_t->cls = TYPE_PRIMITIVE;
    
    switch (s_curr.type) {
        case TOK_U8: base_t->primitive = PRIM_U8; advance(); break;
        case TOK_U16: base_t->primitive = PRIM_U16; advance(); break;
        case TOK_U32: base_t->primitive = PRIM_U32; advance(); break;
        case TOK_U64: base_t->primitive = PRIM_U64; advance(); break;
        case TOK_I8: base_t->primitive = PRIM_I8; advance(); break;
        case TOK_I16: base_t->primitive = PRIM_I16; advance(); break;
        case TOK_I32: base_t->primitive = PRIM_I32; advance(); break;
        case TOK_I64: base_t->primitive = PRIM_I64; advance(); break;
        case TOK_BOOL: base_t->primitive = PRIM_BOOL; advance(); break;
        case TOK_F32: base_t->primitive = PRIM_F32; advance(); break;
        case TOK_F64: base_t->primitive = PRIM_F64; advance(); break;
        case TOK_VOID: base_t->primitive = PRIM_V; advance(); break;
        case TOK_IMPL:
            if (!parse_impl_pattern_type(base_t)) { free(base_t); return NULL; }
            break;
        case TOK_STRUCT:
        case TOK_UNION:
        case TOK_ENUM:
            if (!parse_anonymous_aggregate_type(base_t)) { free(base_t); return NULL; }
            break;
        case TOK_LPAREN: {
            // Parenthesized type: `(type)`. Grouping closes a `fn(...)` return
            // type explicitly so postfix can bind to the whole function type,
            // e.g. `(fn(u32) u32)[4]` = array of 4 fn-pointers (vs the bare
            // `fn(u32) u32[4]` which greedily reads u32[4] as the return type).
            //
            // SOFT-FAIL: a `(` does not guarantee a type — at statement start,
            // `(expr)` is a parenthesized-expression statement, not a declaration.
            // If the inner isn't a type (or no closing `)`), restore the scanner
            // and return NULL so the caller falls back to expression parsing,
            // instead of hard-erroring. (parse_statement tries parse_type first.)
            LexerState lp_save;
            Lexer_Save(&lp_save);
            Token lp_tok = s_curr;
            advance();
            free(base_t);
            base_t = parse_type();
            if (!base_t || s_curr.type != TOK_RPAREN) {
                Lexer_Restore(&lp_save);
                s_curr = lp_tok;
                s_curr_newline_before = false; // conservative; only matters mid-postfix
                return NULL;
            }
            advance();
            break;
        }
        case TOK_FN:
            parse_function_type(base_t);
            break;
        case TOK_IDENTIFIER: {
            Type* res = parse_alias_or_type_param_type(base_t);
            if (!res) return NULL;
            base_t = res;
            break;
        }
        default:
            free(base_t);
            return NULL;
    }
    
    // Unified postfix loop. `*` and `[N]` are postfix operators that bind in
    // source order, so both orderings are now expressible and mean what they
    // read as:
    //   u32*[4]  = array(4) of (u32*)         (star binds first, array wraps it)
    //   u32[4]*  = pointer to (u32[4])         (array binds first, star wraps it)
    //   u32[2][3]= array(2) of array(3) of u32 (leftmost bracket = OUTERMOST)
    // A `*` wraps the accumulated type immediately (outer pointer). A *run* of
    // consecutive `[]` is gathered, then folded right-to-left so the leftmost
    // dim ends up outermost — the convention indexing peels (g[i] selects the
    // leftmost dim). `type[]` (empty) infers size from the literal; 0 = sentinel.
    while (1) {
        // While parsing `new`'s type, a postfix that starts a new line is the next
        // statement (`new u32` <nl> `*x = 42`), not part of this type.
        if (s_new_type_no_nl_postfix && s_curr_newline_before &&
            (s_curr.type == TOK_STAR || s_curr.type == TOK_LBRACKET)) break;
        if (s_curr.type == TOK_STAR) {
            advance();
            base_t = make_pointer_type(base_t);
        } else if (allow_array && s_curr.type == TOK_LBRACKET) {
            // Gather the contiguous run of dimensions, then fold right-to-left.
            uint64_t dims[8];
            struct ASTNode* dim_exprs[8];
            const char* dim_size_params[8] = {0}; // match-pattern size wildcards (`E[N]`), else NULL
            int ndim = 0;
            while (s_curr.type == TOK_LBRACKET) {
                advance();
                if (ndim >= 8) parse_error("too many array dimensions");
                if (s_curr.type == TOK_RBRACKET) {
                    dims[ndim] = 0; // inferred (must be followed by a literal)
                    dim_exprs[ndim] = NULL;
                    ndim++;
                } else if (s_in_match_pattern && s_curr.type == TOK_IDENTIFIER &&
                           param_kind_lookup(s_curr.start, s_curr.length, NULL) < 0 &&
                           !struct_find_by_token_text(s_curr.start, s_curr.length)) {
                    // `E[N]` in a match pattern where N is an undeclared identifier:
                    // N is a SIZE wildcard. Record its name on the array type so
                    // reflect_unify binds it to the concrete array's count (as a
                    // pinned value, like any const-generic value param). We stash it
                    // in size_param via a sentinel dim; the fold happens below.
                    const char* wname = match_wildcard_lookup(s_curr.start, s_curr.length);
                    if (!wname) wname = register_match_wildcard_kind(s_curr.start, s_curr.length, true);
                    advance();
                    dims[ndim] = 0;
                    dim_exprs[ndim] = NULL;
                    // Carry the wildcard name out of this loop via a parallel array.
                    // (dim_size_params mirrors dims/dim_exprs; declared just above.)
                    dim_size_params[ndim] = wname;
                    ndim++;
                    if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after array size wildcard");
                    advance();
                    continue;
                } else {
                    ASTNode* sz_expr = parse_expr_prec(0);
                    int64_t n;
                    // If the size mentions an in-scope generic param, defer it — do
                    // NOT fold now, or a same-named global const would capture it.
                    // Type_Substitute re-folds under the generic frame (param shadows
                    // global), which is the correct pinning precedence.
                    if (expr_mentions_generic_param(sz_expr)) {
                        dims[ndim] = 0;
                        dim_exprs[ndim] = sz_expr;
                    } else if (!ConstEval(sz_expr, &n)) {
                        dims[ndim] = 0;
                        dim_exprs[ndim] = sz_expr;
                    } else {
                        if (n <= 0) parse_error("array size must be positive");
                        dims[ndim] = (uint64_t)n;
                        dim_exprs[ndim] = NULL;
                    }
                    ndim++;
                }
                if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after array size");
                advance();
            }
            for (int i = ndim - 1; i >= 0; i--) {
                base_t = make_array_type(base_t, dims[i], dim_exprs[i], dim_size_params[i]);
            }
        } else {
            break;
        }
    }

    return base_t;
}

// Default type parse: allows the array suffix (type[N]). `new` uses the
// no-array form so it can parse a runtime [count] itself.
static Type* parse_type(void) { return parse_type_ex(true); }

static bool token_is_type_start(TokenType t) {
    switch (t) {
        case TOK_U8: case TOK_U16: case TOK_U32: case TOK_U64:
        case TOK_I8: case TOK_I16: case TOK_I32: case TOK_I64:
        case TOK_BOOL: case TOK_F32: case TOK_F64: case TOK_VOID:
        case TOK_FN:
        // Anonymous aggregate types in type position: `struct { ... }`, `union { ... }`,
        // `enum { ... }`. Each of these keywords ALSO opens a declaration
        // (`struct Name { ... }`), but that is not a conflict: parse_type_ex's shared
        // anon-aggregate case soft-fails (returns NULL, consuming nothing) unless a `{`
        // follows immediately, so `union Bits { ... }` at top level is rejected as a
        // type and falls through to the declaration path exactly as `struct Name { ... }`
        // already does. That is why these need no s_in_match_pattern gate, unlike
        // TOK_IMPL below -- `impl` has no `{`-immediately-follows form as a declaration.
        case TOK_STRUCT:
        case TOK_UNION:
        case TOK_ENUM:
            return true;
        case TOK_IMPL:
            // `impl {...}` is a type only as a MATCH PATTERN. Everywhere else `impl`
            // opens a declaration (`impl P { ... }`), so admitting it as a type-start
            // unconditionally would make every impl block try to parse as a type.
            // Gated on the same flag that turns undeclared identifiers into wildcards.
            return s_pattern_types_ok;
        default:
            return false;
    }
}

static int get_token_prec(TokenType type) {
    switch (type) {
        case TOK_EQ:
        case TOK_PLUS_EQ: case TOK_MINUS_EQ: case TOK_STAR_EQ: case TOK_SLASH_EQ: case TOK_MOD_EQ:
        case TOK_AMP_EQ: case TOK_PIPE_EQ: case TOK_CARET_EQ: case TOK_SHL_EQ: case TOK_SHR_EQ:
            return 1;
        case TOK_OROR: return 2;
        case TOK_ANDAND: return 3;
        case TOK_EQEQ:
        case TOK_NEQ: return 4;
        case TOK_LT:
        case TOK_GT:
        case TOK_LTE:
        case TOK_GTE: return 5;
        case TOK_PIPE: return 6;
        case TOK_CARET: return 7;
        case TOK_AMP: return 8;
        case TOK_SHL:
        case TOK_SHR: return 9;
        case TOK_PLUS:
        case TOK_MINUS: return 10;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_MOD: return 11;
        default: return -1;
    }
}

static ASTNodeType get_op_node_type(TokenType type) {
    switch (type) {
        case TOK_PLUS: return AST_ADD;
        case TOK_MINUS: return AST_SUB;
        case TOK_STAR: return AST_MUL;
        case TOK_SLASH: return AST_DIV;
        case TOK_MOD: return AST_MOD;
        case TOK_AMP: return AST_BIT_AND;
        case TOK_PIPE: return AST_BIT_OR;
        case TOK_CARET: return AST_BIT_XOR;
        case TOK_SHL: return AST_SHL;
        case TOK_SHR: return AST_SHR;
        case TOK_EQEQ: return AST_EQ;
        case TOK_NEQ: return AST_NEQ;
        case TOK_LT: return AST_LT;
        case TOK_GT: return AST_GT;
        case TOK_LTE: return AST_LTE;
        case TOK_GTE: return AST_GTE;
        case TOK_ANDAND: return AST_LOGICAL_AND;
        case TOK_OROR: return AST_LOGICAL_OR;
        case TOK_EQ: return AST_ASSIGN;
        default: parse_error("Unknown operator"); return AST_ADD;
    }
}

static ASTNode* parse_expr_prec(int min_prec);
static ASTNode* parse_statement(void);
static ASTNode* parse_postfix(void);

static ASTNode* parse_block_body(void) {
    return parse_braced_block(true, true);
}

// Does the current token begin a type? Primitive keyword, or a bare identifier
// that names a registered struct (used to disambiguate sizeof(type) vs sizeof(expr)).
// Does an arbitrary token begin a type? Handles the keyword starts plus
// identifiers that name an in-scope type parameter or a registered struct.
// A '(' begins a type only when what follows it begins a type too (so a
// parenthesized *type* `(fn()u32)` is distinguished from a parenthesized
// *expression* `(x+1)` without committing — see grouping base in parse_type_ex).
// Does a token begin a type, ignoring leading parens? Primitive keyword, `fn`,
// or an identifier naming an in-scope type-param / registered struct.
static bool token_begins_type(Token t) {
    if (token_is_type_start(t.type)) return true;
    if (t.type == TOK_IDENTIFIER) {
        if (type_param_lookup(t.start, t.length)) return true;
        if (struct_find_by_token_text(t.start, t.length) != NULL) return true;
        if (alias_lookup(t.start, t.length) != NULL) return true;
        // Inside a `match T` arm pattern, a bare undeclared identifier is a
        // wildcard, which is a valid type-start — this is what lets a function
        // pattern's return type wildcard parse (`fn(A) B`: B begins the return
        // type) instead of being mistaken for the arm body.
        if (s_in_match_pattern) return true;
    }
    return false;
}

// Does the current token begin a type? A leading run of `(` is skipped, so
// `(((i32)))` is recognized just like `i32` — parenthesizing a type is
// idempotent. The decision keys on the first non-`(` token: a *type* keyword/
// name means "type" (cast / parenthesized type), anything else (a value, e.g.
// `(x + 1)`) means "expression". Scans ahead on the lexer and rewinds, so it
// has no side effects on the parse position.
static bool curr_begins_type(void) {
    if (token_begins_type(s_curr)) return true;
    if (s_curr.type != TOK_LPAREN) return false;
    LexerState save;
    Lexer_Save(&save);
    Token t = s_curr;
    while (t.type == TOK_LPAREN) t = Lexer_NextToken();
    bool is_type = token_begins_type(t);
    Lexer_Restore(&save);
    return is_type;
}

// Does the current identifier name a registered enum (concrete or generic template)?
// Used to route `EnumName.Variant{..}` to enum construction in primary position.
static bool curr_names_enum(void) {
    if (s_curr.type != TOK_IDENTIFIER) return false;
    StructDef* sd = struct_find_by_token_text(s_curr.start, s_curr.length);
    if (!sd || !sd->is_enum) return false;
    // Only the genuinely deprecated `EnumName.Variant{...}` SHAPE should hit
    // the migration error — a bare enum name with nothing after it (used as
    // an ordinary type reference, e.g. `T == Color`) is legitimate and was
    // never what this check was meant to catch. Peek one token ahead without
    // consuming (same save/restore pattern curr_begins_type already uses).
    LexerState save;
    Lexer_Save(&save);
    Token next = Lexer_NextToken();
    Lexer_Restore(&save);
    return next.type == TOK_DOT;
}

// Parse the body of a struct literal: `.field = expr, .field = expr, ...}`.
// Opening '{' already consumed by the caller; consumes through the closing
// '}'. sdef is left unset (NULL) — callers that already know the concrete
// struct (e.g. `new T{...}`) set it afterward; callers that don't (a bare
// `{...}` whose type comes from context) leave it for typecheck to resolve.
// This was duplicated wholesale between new T{...} and the bare-literal
// parser — same three parallel malloc'd arrays, same grow-on-overflow
// doubling, same s_in_struct_literal_field save/restore around the value
// parse — differing only in what happened to sdef before/after the loop.
static ASTNode* parse_struct_literal_body(void) {
    ASTNode* node = new_node(AST_STRUCT_LITERAL);
    node->struct_lit.sdef = NULL;
    size_t cap = 8;
    node->struct_lit.field_names = malloc(cap * sizeof(char*));
    node->struct_lit.field_name_lens = malloc(cap * sizeof(size_t));
    node->struct_lit.values = malloc(cap * sizeof(ASTNode*));
    node->struct_lit.count = 0;
    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        if (s_curr.type != TOK_DOT) parse_error("Expected '.field' in struct literal");
        advance();
        if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected field name in struct literal");
        Token fname = s_curr; advance();
        if (s_curr.type != TOK_EQ) parse_error("Expected '=' in struct literal field");
        advance();
        if (node->struct_lit.count >= cap) {
            cap *= 2;
            node->struct_lit.field_names = realloc(node->struct_lit.field_names, cap * sizeof(char*));
            node->struct_lit.field_name_lens = realloc(node->struct_lit.field_name_lens, cap * sizeof(size_t));
            node->struct_lit.values = realloc(node->struct_lit.values, cap * sizeof(ASTNode*));
        }
        node->struct_lit.field_names[node->struct_lit.count] = fname.start;
        node->struct_lit.field_name_lens[node->struct_lit.count] = fname.length;
        bool prev_slf = s_in_struct_literal_field;
        s_in_struct_literal_field = true;
        node->struct_lit.values[node->struct_lit.count] = parse_expr_prec(2); // above assignment, below comma
        s_in_struct_literal_field = prev_slf;
        node->struct_lit.count++;
        if (s_curr.type == TOK_COMMA) advance();
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end struct literal");
    advance();
    return node;
}

// Parse a call's argument list: `expr, expr, ...)`. Opening '(' already
// consumed by the caller; consumes through the closing ')'. Writes the
// parsed argument count to *count_out. This was duplicated identically
// between the direct-call site (`NAME(args)`) and the indirect-call site
// (`(fnptr_expr)(args)`) — same arg_cap=6 malloc, same grow-on-overflow
// doubling, same comma-separated parse_expr_prec(0) loop — differing only
// in which AST_CALL node the result got written into.
static ASTNode** parse_call_arg_list(size_t* count_out) {
    size_t arg_cap = 6, arg_count = 0;
    ASTNode** args = malloc(sizeof(ASTNode*) * arg_cap);
    if (s_curr.type != TOK_RPAREN) {
        while (1) {
            if (arg_count >= arg_cap) {
                arg_cap *= 2;
                args = realloc(args, sizeof(ASTNode*) * arg_cap);
            }
            args[arg_count++] = parse_expr_prec(0);
            if (s_curr.type == TOK_COMMA) advance();
            else break;
        }
    }
    if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' after arguments");
    advance();
    *count_out = arg_count;
    return args;
}

// Compile-time reflection operators: sizeof(), alignof(), fnof(), offsetof(),
// nameof(). Split out of parse_primary, which had grown to 851 lines by absorbing every
// expression form that wasn't an infix operator.
//
// These belong together because they are ONE shape -- `KEYWORD ( type [, arg] )` -- and
// they are the surface cost of a deliberate design choice: Torrent has no `typeinfo`,
// because a type is never a value. Each operator therefore takes a TYPE and returns a
// LEAF (a u64, a u8*, a fn symbol) that cannot be projected further, and each needs its
// own hand-written parse. Five near-identical blocks is what "no types as values" LOOKS
// like at the parser; the alternative is one general reflection API returning a
// projectable value, i.e. the spiral this language exists to avoid.
//
// Returns NULL if the current token starts none of them, so parse_primary can fall
// through to the next form.

static ASTNode* parse_sizeof_expr(void) {
    advance();
    if (s_curr.type != TOK_LPAREN) parse_error("Expected '(' after sizeof");
    advance();
    if (curr_begins_type()) {
        Type* t = parse_type();
        if (!t) parse_error("Expected type in sizeof");
        ASTNode* node = new_node(AST_SIZEOF);
        node->sizeof_expr.type = t;
        node->result_type = NULL;
        if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' after sizeof operand");
        advance();
        return node;
    } else {
        ASTNode* e = parse_expr_prec(0);
        ASTNode* node = new_node(AST_SIZEOF);
        Type* pin;
        if (e->type == AST_IDENT && !e->ident.sym &&
            param_kind_lookup(e->ident.name, e->ident.name_len, &pin) == 1) {
            node->sizeof_expr.type = pin;
            node->sizeof_expr.defer_expr = NULL;
        } else if (expr_mentions_generic_param(e)) {
            node->sizeof_expr.type = NULL;
            node->sizeof_expr.defer_expr = e;
        } else {
            Type* t = Type_Infer(e);
            if (!t) parse_error("sizeof: cannot determine type of expression");
            node->sizeof_expr.type = t;
            node->sizeof_expr.defer_expr = NULL;
        }
        node->result_type = NULL;
        if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' after sizeof operand");
        advance();
        return node;
    }
}

static ASTNode* parse_alignof_expr(void) {
    advance();
    if (s_curr.type != TOK_LPAREN) parse_error("Expected '(' after alignof");
    advance();
    if (!curr_begins_type()) parse_error("Expected a type in alignof");
    Type* t = parse_type();
    if (!t) parse_error("Expected type in alignof");
    ASTNode* node = new_node(AST_ALIGNOF);
    node->sizeof_expr.type = t;
    node->sizeof_expr.defer_expr = NULL;
    node->result_type = NULL;
    if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' after alignof operand");
    advance();
    return node;
}

static ASTNode* parse_offsetof_and_nameof_expr(void) {
    bool is_nameof = (s_curr.type == TOK_NAMEOF);
    advance();
    if (s_curr.type != TOK_LPAREN) parse_error(is_nameof ? "Expected '(' after nameof" : "Expected '(' after offsetof");
    advance();
    if (!is_nameof && !curr_begins_type()) parse_error("Expected a struct type in offsetof");
    Type* t = curr_begins_type() ? parse_type() : NULL;

    if (is_nameof && !t && s_curr.type == TOK_IDENTIFIER) {
        Token ident = s_curr;
        advance();
        if (s_curr.type == TOK_RPAREN) {
            advance();
            ASTNode* node = new_node(AST_STRING);
            node->int_value = (uint64_t)(uintptr_t)strndup(ident.start, ident.length);
            return node;
        }
        parse_error("Expected ')' after nameof identifier");
    } else if (is_nameof && t && s_curr.type == TOK_RPAREN) {
        advance();
        if (s_type_param_count == 0) {
            ASTNode* node = new_node(AST_STRING);
            char buf[256];
            Type_ToString(t, buf, sizeof(buf));
            node->int_value = (uint64_t)(uintptr_t)strdup(buf);
            return node;
        } else {
            ASTNode* node = new_node(AST_NAMEOF);
            node->field_ref_expr.type = t;
            node->field_ref_expr.index_expr = NULL;
            node->result_type = NULL;
            return node;
        }
    }

    if (!t) parse_error(is_nameof ? "Expected struct type in nameof" : "Expected struct type in offsetof");
    if (t->cls != TYPE_STRUCT && t->cls != TYPE_PARAM)
        parse_error(is_nameof ? "nameof's first argument must be a struct type" : "offsetof's first argument must be a struct type");
    if (s_curr.type != TOK_COMMA) parse_error(is_nameof ? "Expected ',' after nameof's type argument" : "Expected ',' after offsetof's type argument");
    advance();
    ASTNode* idx_expr = parse_expr_prec(0);
    if (s_curr.type != TOK_RPAREN) parse_error(is_nameof ? "Expected ')' after nameof operand" : "Expected ')' after offsetof operand");
    advance();

    int64_t idx;
    if (t->cls == TYPE_STRUCT && s_type_param_count == 0 && ConstEval(idx_expr, &idx)) {
        StructDef* sd = Struct_Find(t->struct_name);
        if (!sd) parse_error(is_nameof ? "nameof: unknown struct type" : "offsetof: unknown struct type");
        Struct_Layout(sd);
        if (idx < 0 || (uint64_t)idx >= sd->field_count)
            parse_error(is_nameof ? "nameof: field index out of range" : "offsetof: field index out of range");
        if (is_nameof) {
            ASTNode* node = new_node(AST_STRING);
            node->int_value = (uint64_t)(uintptr_t)strdup(sd->fields[idx].name);
            return node;
        } else {
            ASTNode* node = new_node(AST_INT_LITERAL);
            node->lit_kind = LIT_INT;
            node->int_value = sd->fields[idx].offset;
            return node;
        }
    }

    ASTNode* node = new_node(is_nameof ? AST_NAMEOF : AST_OFFSETOF);
    node->field_ref_expr.type = t;
    node->field_ref_expr.index_expr = idx_expr;
    node->result_type = NULL;
    return node;
}
static ASTNode* parse_reflect_op(void) {
    // sizeof(type) or sizeof(expr) -> u64 compile-time constant. Polymorphic like C.
    if (s_curr.type == TOK_SIZEOF) return parse_sizeof_expr();

    // alignof(T) -> u64 compile-time constant, backed by Type_AlignOf. Unlike
    // sizeof, only the type form is supported (matches the spec: alignment is
    // a property of a TYPE, not a value). Never folds at parse time, even for
    // a fully concrete T -- exactly mirroring sizeof(type)'s own behavior:
    // it always builds the node and lets ConstEval/codegen resolve it, so a
    // generic-param T (Type_Substitute'd later in clone_ast) and a concrete T
    // share one code path.
    if (s_curr.type == TOK_ALIGNOF) return parse_alignof_expr();

    // offsetof(StructType, i) -> u64 byte offset of the i-th field (0-based,
    // declaration order; for an enum this is just TAG_SIZE for every i, since
    // that's already what every field's .offset is set to). nameof(StructType, i)
    // -> u8* to the i-th field's name, as a static string.
    //
    // Both share the same parse shape, differing only in what they extract from
    // the resolved StructField and what AST node the result folds into. When
    // BOTH the struct type and the field index are already concrete right here
    // (the common, non-generic case), resolve immediately: offsetof becomes a
    // plain AST_INT_LITERAL and nameof becomes a plain AST_STRING, exactly as
    // if the user had written the literal/string by hand -- no AST_OFFSETOF or
    // AST_NAMEOF node is ever built for that case. Only when the struct type is
    // still a generic type-param, or the index expression references an
    // in-scope generic const param, does this defer: an AST_OFFSETOF/AST_NAMEOF
    // node is built instead, to be resolved once instantiation makes both
    // concrete (AST_OFFSETOF lazily via ConstEval, mirroring sizeof's
    // defer_expr; AST_NAMEOF eagerly in clone_ast, since strings can't flow
    // through ConstEval in this compiler).
    if (s_curr.type == TOK_OFFSETOF || s_curr.type == TOK_NAMEOF) return parse_offsetof_and_nameof_expr();


    return NULL;
}

// `new T` / `new T{...}` / `new T[n]`. Split out of parse_primary.
static ASTNode* parse_new_expr(void) {

    // new T  /  new T{...}  /  new T[expr]  -> typed pointer (T*). Expression-only.
    // Parses a TYPE (resolving the type/value ambiguity), then ONE optional suffix:
    // a struct/array literal initializer, or a runtime [count]. Never both. Size is
    // always computed from the type via Type_SizeOf at codegen, so this composes to
    // arbitrarily nested and (later) generic types with no special-casing here.
    if (s_curr.type == TOK_NEW) {
        advance();
        ASTNode* count = NULL;
        if (s_curr.type == TOK_LBRACKET) {
            advance();
            count = parse_expr_prec(0);
            if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after 'new['");
            advance();
        }

        s_new_type_no_nl_postfix = true;
        Type* t = parse_type_ex(true); // Base type + postfixes ok!
        s_new_type_no_nl_postfix = false;
        
        if (!t) parse_error("Expected type after 'new'");
        
        ASTNode* node = new_node(AST_NEW);
        node->new_expr.alloc_type = t;
        node->new_expr.init = NULL;
        node->new_expr.count = count;
        
        if (s_curr.type == TOK_LBRACE) {
            if (count != NULL) parse_error("Cannot initialize an array allocation with a struct literal");
            
            // new T{...}: an initializer for a single object. Parse the SAME bare
            // literal shape a plain `T x = {...}` declaration would, then resolve
            // it through resolve_brace_literal — the one general path every other
            // struct-literal site in this file uses (const decl, global decl,
            // call args, return, cast, assignment...) — rather than hand-setting
            // sdef directly. Hand-setting only fixed up the literal's OWN sdef;
            // resolve_brace_literal also recurses into each field's value and
            // resolves any NESTED bare `{...}` against the field's declared type
            // (types.c's AST_STRUCT_LITERAL case, line ~392-410). Without that
            // recursion, `new Outer{.in = {.a=1,.b=2}}` left the inner literal's
            // sdef NULL, which layout_fill's recursive walk then hit and bailed
            // on — invisible at runtime (Typecheck_Tree's own AST_STRUCT_LITERAL
            // case did this same recursion, just too late for a comptime fold,
            // which runs at parse time, before typecheck).
            if (t->cls != TYPE_STRUCT) parse_error("'new T{...}' initializer requires T to be a struct");
            advance(); // consume '{'
            ASTNode* lit = parse_struct_literal_body();
            resolve_brace_literal(lit, t);
            node->new_expr.init = lit;
        }

        node->result_type = NULL; // pointer-to-t; inferred in Type_Infer
        return node;
    }

    return NULL;
}

// `if` in EXPRESSION position (a value, not a statement). Split out of parse_primary.
static ASTNode* parse_if_expr(void) {

    if (s_curr.type == TOK_IF) {
        advance();

        ASTNode* condition = parse_expr_prec(0);
        ASTNode* true_block = parse_block_body();
        ASTNode* false_block = NULL;
        if (s_curr.type == TOK_ELSE) {
            advance();
            if (s_curr.type == TOK_IF) {
                // Flattened else if: parse the 'if' chain recursively. Direct call,
                // not parse_expr_prec(0) -- `if` is no longer reachable from the
                // general expression parser (parse_primary).
                false_block = parse_if_expr();
            } else {
                false_block = parse_block_body();
            }
        }
        ASTNode* node = new_node(AST_IF);
        node->if_stmt.condition = condition;
        node->if_stmt.true_block = true_block;
        node->if_stmt.false_block = false_block;
        return node;
    }

    return NULL;
}

// Bare, payload-less enum-variant node shell: `.Variant` with the type still
// inferred from context (resolved from target in typecheck). Shared by every
// `.Variant` / `.Variant(payload)` parse site; callers that allow a payload
// fill node->struct_lit.values[0]/count themselves after checking for one --
// what differs between sites is how (or whether) a following '{' is read as
// a payload wrapper, not how the bare node is built.
static ASTNode* make_enum_variant_node(Token vtok) {
    ASTNode* node = new_node(AST_STRUCT_LITERAL);
    node->struct_lit.sdef = NULL;            // resolved from target in typecheck
    node->struct_lit.is_enum_variant = true;
    node->struct_lit.field_names = malloc(sizeof(char*));
    node->struct_lit.field_name_lens = malloc(sizeof(size_t));
    node->struct_lit.values = malloc(sizeof(ASTNode*));
    node->struct_lit.field_names[0] = vtok.start;
    node->struct_lit.field_name_lens[0] = vtok.length;
    node->struct_lit.count = 0;              // payload count: 0 or 1
    return node;
}

// Parse `Variant` or `Variant(payload)` with the leading `.` ALREADY consumed
// by the caller. Builds the enum-variant node and fills its optional single
// `(payload)`. Shared by parse_enum_literal and parse_match_value's arm head.
static ASTNode* parse_enum_variant_after_dot(void) {
    if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected variant name after '.'");
    Token vtok = s_curr; advance();
    ASTNode* node = make_enum_variant_node(vtok);
    if (s_curr.type == TOK_LPAREN) {
        advance();
        if (s_curr.type != TOK_RPAREN) {
            node->struct_lit.values[0] = parse_expr_prec(0);
            node->struct_lit.count = 1;
        }
        if (s_curr.type != TOK_RPAREN) parse_error("Expected ')' after variant payload");
        advance();
    }
    return node;
}

// `.Variant` / `.Variant(payload)` -- an enum literal with the type inferred from
// context. Split out of parse_primary.
static ASTNode* parse_enum_literal(void) {


    // Contextual enum literal `.Variant` / `.Variant(payload)` -- UNTYPED at parse
    // time, just like a bare `{...}` aggregate. The leading `.` is unambiguous (no
    // value expression starts with `.`), so the enum type is inferred from the
    // target via resolve_brace_literal rather than written out.
    //
    // Payload uses `(` (not `{`) deliberately: a variant is a tagged constructor
    // taking one positional argument, not an aggregate with named/positional
    // fields, so it doesn't share {}'s grammar family with struct/array literals.
    // This also means a bare `.Variant` can never be followed by a `{` that
    // belongs to IT -- the `{` is always something else (a block, a struct
    // literal elsewhere), so no lookahead is needed here or at any call site
    // to tell "payload" from "whatever comes after this value." `(` was
    // previously `{`, which required match's own depth-counting lookahead hack
    // to avoid swallowing the arm's own body brace on a payload-less variant --
    // that hack, and the equivalent bug this left unpatched in if/while/every
    // other expression position, is exactly what this syntax removes.
    if (s_curr.type == TOK_DOT) {
        advance();
        return parse_enum_variant_after_dot();
    }

    // Bare aggregate literal `{...}` -- UNTYPED at parse time. Its concrete type is
    // inferred from the target (decl var_type, fn return type, param type, field type)
    // during typecheck via resolve_brace_literal. There is NO type prefix: the target
    // always supplies the type (every legal position has one), and a cast handles the
    // rare untyped-position case. We peek to decide the SHAPE only:
    //   `{ .field = ... }`  -> struct-shaped (AST_STRUCT_LITERAL, sdef=NULL)
    //   `{ expr, ... }` / `{}` -> array-shaped (AST_ARRAY_LITERAL, elem_type=NULL)
    // A leading `.IDENT` is AMBIGUOUS: `.field =` is a struct field, but `.Variant`
    // (no `=`) is a contextual ENUM element of an ARRAY literal — e.g.
    // `Opt[2] a = {.S(10), .None}`. Two-token lookahead disambiguates: `.IDENT =`
    // is struct-shaped, anything else after `.IDENT` is an enum element (array).

    return NULL;
}


// An identifier in expression position: a plain variable/const reference, a generic
// function reference (`foo[T]`, called or as a VALUE), or a call. Split out of
// parse_primary -- this branch alone was 234 lines, over half of what remained.
//
// Long because a bare identifier is the single most overloaded token in the grammar:
// it can be a value, a type name, a generic function, an enum, or a const, and `[` after
// it can mean generic args OR an index. Every one of those is resolved here, by lookahead
// (curr_begins_type / gsig_find / the newline guard), which is the standing cost of a
// type-first grammar that also has `[]` indexing.
//
// Returns NULL if the current token is not an identifier.

static ASTNode* parse_explicit_generic_call(Symbol* sym, Token id_tok, bool* out_is_gcall) {
    const char** call_tparams = NULL;
    Type**       call_pkinds  = NULL;
    size_t       call_pcount  = 0;
    bool         is_gcall     = false;
    if (s_curr.type == TOK_LBRACKET && !s_curr_newline_before) {
        if (sym && sym->kind == SYM_FUNCTION && sym->generic_decl) {
            ASTNode* gd  = sym->generic_decl;
            call_tparams = gd->func_decl.type_params;
            call_pkinds  = gd->func_decl.param_kinds;
            call_pcount  = gd->func_decl.type_param_count;
            is_gcall     = true;
        } else if (!sym) {
            struct GenericSig* gs = gsig_find(id_tok.start, id_tok.length);
            if (gs) {
                call_tparams = gs->tparams;
                call_pkinds  = gs->pkinds;
                call_pcount  = gs->pcount;
                is_gcall     = true;
            }
        }
    }
    *out_is_gcall = is_gcall;
    if (!is_gcall) return NULL;

    advance(); // consume '['
    Type** targs; size_t tcount;
    parse_generic_arg_list(call_tparams, call_pkinds, call_pcount, &targs, &tcount);
    if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after explicit generic call arguments");
    advance();
    if (call_pcount && tcount != call_pcount)
        parse_error("wrong number of explicit generic arguments for this call");

    if (s_curr.type != TOK_LPAREN) {
        ASTNode* id = new_node(AST_IDENT);
        id->ident.name = id_tok.start;
        id->ident.name_len = id_tok.length;
        id->ident.sym = sym;
        id->ident.type_args = targs;
        id->ident.type_arg_count = tcount;
        return id;
    }

    advance();
    ASTNode* node = new_node(AST_CALL);
    node->call.target_name = id_tok.start;
    node->call.target_name_len = id_tok.length;
    node->call.args = parse_call_arg_list(&node->call.arg_count);
    node->call.sym = sym;
    node->call.target_expr = NULL;
    node->call.type_args = targs;
    node->call.type_arg_count = tcount;
    return node;
}
static ASTNode* parse_ident_expr(void) {
    if (s_curr.type == TOK_IDENTIFIER) {
        Token id_tok = s_curr;
        advance();
        // NOTE: there is no `Name{...}` prefixed struct-literal form anymore. A struct
        // literal is written bare `{...}` and its type is inferred from the target (see
        // the TOK_LBRACE primary above + resolve_brace_literal in typecheck). Generic
        // struct literals likewise: the target type `Pair[u32,bool]` instantiates, and
        // the bare `{...}` binds to it. The IDENT here is a variable / function / call.
        Symbol* sym = SymTable_Find(s_symtable, id_tok.start, id_tok.length);
        // Explicit generic call-site type/value arguments: `NAME[T, N, ...](args)`.
        // Reintroduced escape hatch for the one accepted gap in ordinary inference
        // (§17.2/§4 of the language doc): a generic function whose type parameter
        // appears in neither a typed argument nor the return type has no target to
        // cast against and is otherwise uncallable. Requires the generic function to
        // already be declared above this call site, because parsing a mixed
        // type/value bracket list correctly (kind-driven, no greedy-type-param
        // guessing) needs the callee's declared param_kinds -- the same requirement
        // generic STRUCT instantiation already has.
        // Explicit generic call-site type/value arguments: `NAME[T, N, ...](args)`.
        // The bracket list is parsed KIND-DRIVEN -- each slot is a type or a value, and
        // only the callee's declaration says which -- so the callee's param_kinds must be
        // available here. They come from one of two places:
        //   * sym->generic_decl, when the callee was declared ABOVE this call, or
        //   * the pass-0b signature table, when it is declared BELOW (forward reference).
        // Pass 0b pre-parses every generic header with the same parse_generic_param_list,
        // so a forward explicit call now works exactly like a forward ordinary call
        // already did. Before it existed, only the first case was possible, and the
        // second silently fell through to the postfix loop and became an array index.
        bool is_gcall = false;
        ASTNode* gcall_node = parse_explicit_generic_call(sym, id_tok, &is_gcall);
        if (is_gcall) return gcall_node;
        // An UNKNOWN identifier followed by `[` cannot be an index -- there is nothing
        // to index. In practice it is an explicit generic call whose callee is declared
        // further down the file (`b[T](x)` with `fn b[T]` below). Without this check the
        // postfix loop silently turns it into an AST_INDEX and the failure surfaces much
        // later in Type_Infer as "indexing a non-array, non-pointer", naming the wrong
        // construct entirely and hiding the real cause.
        //
        // EXCEPT: an in-scope generic PARAMETER is also absent from the symbol table and
        // is legitimately indexable -- an array-typed value param supports `W[0]`
        // (testsuite/cgen/agg_array_param_indexed). So exclude anything that names a
        // type/value param currently in scope, and only then conclude "forward generic
        // call". Getting this wrong turns a valid index into a spurious error.
        //
        // The prior-declaration requirement itself is deliberate, not a limitation to
        // route around: a mixed type/value bracket list (`f[T, 4]`) is parsed kind-driven,
        // so it needs the callee's param_kinds to know whether each slot is a type or a
        // value. Guessing is exactly what this language does not do. Say so, and point at
        // the two ways out.
        if (s_curr.type == TOK_LBRACKET && !s_curr_newline_before && !sym) {
            bool is_gparam = false;
            for (size_t gi = 0; gi < s_type_param_count; gi++) {
                if (s_type_params[gi] &&
                    strlen(s_type_params[gi]) == id_tok.length &&
                    strncmp(s_type_params[gi], id_tok.start, id_tok.length) == 0) {
                    is_gparam = true; break;
                }
            }
            if (!is_gparam && match_wildcard_lookup(id_tok.start, id_tok.length) == NULL) {
                parse_error("explicit generic call arguments require the callee to be declared "
                            "ABOVE this call (the bracket list is parsed kind-driven, so it needs "
                            "the callee's declared parameter kinds). Either move the callee above, "
                            "or drop the brackets and let the type arguments be inferred -- inferred "
                            "calls resolve fine against a callee declared later");
            }
        }
        // Generic functions are called like normal functions: `NAME(args)`.
        // Type inference automatically resolves the generic parameters.
        bool is_gparam_call = false;
        if (!sym) {
            Type* dummy;
            if (param_kind_lookup(id_tok.start, id_tok.length, &dummy) >= 0) is_gparam_call = true;
        }
        if (s_curr.type == TOK_LPAREN && !s_curr_newline_before && (!sym || sym->kind == SYM_FUNCTION) && !is_gparam_call) {
            // Function call (direct). The `!s_curr_newline_before` guard stops a
            // `(` that opens a NEW statement from gluing onto this identifier as a
            // call: `mk` <newline> `(...)` is two statements, not `mk(...)`.
            advance();
            ASTNode* node = new_node(AST_CALL);
            node->call.target_name = id_tok.start;
            node->call.target_name_len = id_tok.length;
            node->call.args = parse_call_arg_list(&node->call.arg_count);
            // Deferred resolution: a callee may be defined later in the file (no
            // forward declarations needed). Resolved in the typecheck pass once all
            // top-level signatures are known. Resolve now if already visible (REPL).
            node->call.sym = sym;
            node->call.target_expr = NULL;
            return node;
        } else {
            // Variable reference, or a named constant.
            // A generic param in scope shadows any same-named global/outer variable or const.
            // Emit a deferred AST_IDENT (not a folded literal or bound sym) so it resolves
            // under the generic frame at instantiation — correct pinning precedence.
            {
                Type* dummy;
                if (param_kind_lookup(id_tok.start, id_tok.length, &dummy) >= 0) {
                    return make_ident_node(id_tok.start, id_tok.length, NULL);
                }
            }
            if (sym) {
                if (sym->kind == SYM_CONST) {
                    ConstDef* c = sym->cdef;
                    ASTNode* node = new_node(AST_INT_LITERAL);
                    if (c->type && Type_IsFloat(c->type)) {
                        node->lit_kind = LIT_FLOAT;
                        double d; memcpy(&d, &c->value, sizeof d);
                        node->float_value = d;
                    } else {
                        node->lit_kind = LIT_INT;
                        node->int_value = (uint64_t)c->value;
                    }
                    if (c->pending_expr) Const_RegisterPendingUse(node, c);
                    return node;
                }
                return make_ident_node(id_tok.start, id_tok.length, sym);
            }
            // Deferred resolution: the identifier may refer to a function defined
            // later in the file (e.g. function pointer passed as argument).
            // Create the node with sym=NULL; Typecheck_Tree will resolve it.
            {
                ASTNode* node = new_node(AST_IDENT);
                node->ident.name = id_tok.start;
                node->ident.name_len = id_tok.length;
                node->ident.sym = NULL;
                return node;
            }
        }
    }

    return NULL;
}



static ASTNode* parse_brace_literal(void) {
    advance(); // consume '{'
    bool struct_shaped = false;
    if (s_curr.type == TOK_DOT) {
        LexerState sh_save;
        Lexer_Save(&sh_save);
        Token after_dot = Lexer_NextToken();
        Token after_id = (after_dot.type == TOK_IDENTIFIER)
                             ? Lexer_NextToken() : after_dot;
        Lexer_Restore(&sh_save);
        struct_shaped = (after_dot.type == TOK_IDENTIFIER && after_id.type == TOK_EQ);
    }
    if (struct_shaped) {
        return parse_struct_literal_body();
    } else {
        ASTNode* node = new_node(AST_ARRAY_LITERAL);
        node->array_lit.elem_type = NULL;
        size_t cap = 8;
        node->array_lit.values = (ASTNode**)malloc(cap * sizeof(ASTNode*));
        node->array_lit.count = 0;
        while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
            if (node->array_lit.count >= cap) {
                cap *= 2;
                node->array_lit.values = realloc(node->array_lit.values, cap * sizeof(ASTNode*));
            }
            node->array_lit.values[node->array_lit.count++] = parse_expr_prec(2);
            if (s_curr.type == TOK_COMMA) advance();
        }
        if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end array literal");
        advance();
        return node;
    }
}
static ASTNode* parse_primary(void) {
    ASTNode* r = parse_reflect_op();
    if (r) return r;
    { ASTNode* r = parse_new_expr();     if (r) return r; }
    { ASTNode* r = parse_enum_literal(); if (r) return r; }

    if (s_curr.type == TOK_LBRACE) return parse_brace_literal();
    if (s_curr.type == TOK_INTEGER) {
        ASTNode* node = make_int_literal(s_curr.int_value, LIT_INT);
        advance();
        return node;
    } else if (s_curr.type == TOK_FLOAT) {
        ASTNode* node = new_node(AST_INT_LITERAL);
        node->lit_kind = LIT_FLOAT;
        node->float_value = s_curr.float_value;
        advance();
        return node;
    } else if (s_curr.type == TOK_STRING) {
        ASTNode* node = new_node(AST_STRING);
        node->int_value = s_curr.int_value; // pointer to the static bytes
        advance();
        return node;
    } else if (s_curr.type == TOK_TRUE || s_curr.type == TOK_FALSE) {
        ASTNode* node = make_int_literal((s_curr.type == TOK_TRUE) ? 1 : 0, LIT_BOOL);
        advance();
        return node;
    } else if (s_curr.type == TOK_NULL) {
        ASTNode* node = make_int_literal(0, LIT_NULL);
        advance();
        return node;
    } else if (s_curr.type == TOK_IDENTIFIER && curr_names_enum()) {
        // Qualified enum construction `EnumType.Variant{..}` has been REMOVED in favor
        // of the leading-dot form `.Variant{..}`, which infers the enum type from the
        // target (matching how struct literals `{...}` infer their type). This keeps
        // aggregate construction uniform: no type name in front of either. Emit a
        // clear migration error rather than a confusing parse failure.
        Token etok = s_curr;
        parse_error("enum construction no longer takes the type name; "
                    "write `.Variant{..}` instead of `EnumType.Variant{..}` "
                    "(the enum type is inferred from context)");
        (void)etok;
    } else if (s_curr.type != TOK_LPAREN && curr_begins_type()) {
        // A bare type — a primitive keyword (i32, f64, void, fn...), a
        // generic type-param (T), or a registered struct/enum name — used
        // directly in expression position (T == i32, MyStruct == OtherType,
        // etc). Reuses curr_begins_type/parse_type wholesale rather than
        // re-deriving "is this a type" a second time: the same lookahead
        // that already decides this for sizeof(...), casts, and `new T`
        // now also governs whether an ordinary expression starts as a type.
        //
        // Explicitly excludes TOK_LPAREN: `(type)` is already handled by
        // the dedicated cast-parser further below, which does a careful
        // speculative parse-and-rewind to disambiguate a genuine cast from
        // a parenthesized expression whose INNER content merely starts with
        // something that looks like a type (`((u8)(300))`). This branch
        // firing on `(` first stole every parenthesized cast before that
        // logic ever ran — found by tracing curr_begins_type()'s actual
        // firing token on a real regression, not by inspection alone.
        //
        // Guard: a registered STRUCT name alone is ambiguous with a value
        // constructor in some grammars, but Torrent has no `Name{...}`
        // prefixed struct-literal form (removed — see the comment on the
        // plain TOK_IDENTIFIER branch below), so a bare struct name can
        // never legitimately start a value expression here; there is
        // nothing this shadows.
        Type* t = parse_type();
        if (t) {
            ASTNode* node = new_node(AST_TYPE_EXPR);
            node->sizeof_expr.type = t;
            return node;
        }
        // curr_begins_type() said yes but parse_type() couldn't actually
        // parse one (shouldn't normally happen — fall through to the
        // ordinary identifier/primitive handling below rather than error
        // out here, in case some case isn't covered yet).
    }
    { ASTNode* r = parse_ident_expr(); if (r) return r; }
    if (s_curr.type == TOK_LPAREN) {
        // A `(` here is either a CAST `(type) expr` or a parenthesized expression
        // `(expr)`. curr_begins_type() narrows it, but `((u8)(300))` is a paren-
        // EXPRESSION whose inner is a cast — and the `(`-lookahead can mistake the
        // inner `(u8)` for the start of a parenthesized type. So we TRY the cast
        // parse non-committally: snapshot, parse a type, and require a closing `)`
        // immediately after; if that doesn't hold, restore and parse as an
        // expression instead. (parse_type's grouping base soft-fails to NULL, and
        // this catches the "parsed a type but it's really a cast-expr" case.)
        advance();
        if (curr_begins_type()) {
            LexerState type_save;
            Lexer_Save(&type_save);
            Token after_lp = s_curr;
            Type* target = parse_type();
            if (target && s_curr.type == TOK_RPAREN) {
                // Genuine cast: `(type)` then an operand.
                advance();
                ASTNode* node = new_node(AST_CAST);
                node->cast.target_type = target;
                node->cast.expr = parse_postfix();
                return node;
            }
            // Not a clean cast (e.g. `((u8)(300))`): rewind to just after `(` and
            // fall through to expression grouping.
            Lexer_Restore(&type_save);
            s_curr = after_lp;
        }
        // Parenthesized grouping: `(expr)`.
        ASTNode* node = parse_expr_prec(0);
        if (s_curr.type == TOK_RPAREN) {
            advance();
        } else {
            parse_error("Expected ')'");
        }
        return node;
    } else if (s_curr.type == TOK_MINUS) {
        // Unary minus: -x  ==  0 - x  (reuses AST_SUB codegen + constexpr folding).
        advance();
        ASTNode* zero = new_node(AST_INT_LITERAL); zero->lit_kind = LIT_INT; zero->int_value = 0;
        ASTNode* node = new_node(AST_SUB);
        node->binary.left = zero;
        node->binary.right = parse_postfix();
        return node;
    } else if (s_curr.type == TOK_BANG) {
        advance();
        ASTNode* node = new_node(AST_LOGICAL_NOT);
        node->unary = parse_postfix();
        return node;
    } else if (s_curr.type == TOK_TILDE) {
        advance();
        ASTNode* node = new_node(AST_BIT_NOT);
        node->unary = parse_postfix();
        return node;
    } else if (s_curr.type == TOK_STAR) {
        advance();
        ASTNode* node = new_node(AST_DEREF);
        node->unary = parse_postfix();
        return node;
    } else if (s_curr.type == TOK_AMP) {
        advance();
        ASTNode* node = new_node(AST_ADDR);
        node->unary = parse_postfix();
        return node;
    } else if (s_curr.type == TOK_ERROR) {
        parse_error("Invalid token");
    } else if (s_curr.type == TOK_EOF) {
        parse_error("Unexpected end of file in expression");
    } else {
        // Print the token's own source text (every token carries start/length
        // from the lexer regardless of kind) instead of its raw enum integer --
        // "unexpected token 85" told nobody anything; "unexpected token '}'"
        // does. Routed through parse_error for the standard file:line:col
        // format and caret, instead of this site's own bare exit(1).
        char msg[128];
        int len = (int)s_curr.length;
        if (len <= 0) len = 1; // defensive: TOK_EOF-like zero-length tokens
        if (len > 40) len = 40; // clamp: a runaway/garbled token shouldn't blow the buffer
        snprintf(msg, sizeof(msg), "unexpected '%.*s' in expression", len, s_curr.start);
        parse_error(msg);
    }
    return NULL;
}

static ASTNode* parse_postfix(void) {
    ASTNode* node = parse_primary();
    // Postfix chain: field access p.x and indexing a[i] and calls p(), freely mixed.
    // A `(` or `[` that begins a new line does NOT continue the chain: it starts a
    // new statement (Torrent ends statements at the newline). Without this, the
    // value ending one statement glues onto the `(`/`[` opening the next, parsing
    // `mk \n (fn..)` as the call `mk(fn..)`. `.` never starts a statement, so it
    // may still continue across a newline (method-chain style).
    while ((s_curr.type == TOK_DOT && !(s_in_struct_literal_field && s_curr_newline_before))
           || ((s_curr.type == TOK_LBRACKET || s_curr.type == TOK_LPAREN) && !s_curr_newline_before)) {
        if (s_curr.type == TOK_DOT) {
            advance();
            if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected field name after '.'");
            Token fld = s_curr;
            advance();
            ASTNode* fnode = new_node(AST_FIELD);
            fnode->field.base = node;
            fnode->field.field_name = fld.start;
            fnode->field.field_name_len = fld.length;
            fnode->field.field = NULL;
            fnode->field.sdef = NULL;
            node = fnode;

            // `.method[TypeArgs](...)`: explicit generic type arguments on a
            // METHOD call (as opposed to a bare `identity[i32](...)` call,
            // whose own explicit-generic-call parsing lives in
            // parse_explicit_generic_call and requires a resolvable Symbol*
            // at PARSE time -- impossible here, since impl methods aren't
            // resolved to a symbol until typecheck, by mangled-name lookup,
            // long after the parser has moved on). So this can't reuse that
            // path; instead it speculatively parses `[...]` as a type-argument
            // list only when it's immediately followed by `(` -- the same
            // "does the shape that follows disambiguate it" trick used
            // elsewhere in this file (e.g. the cast-vs-parenthesized-expr
            // check above) -- and defers all validation (does this method
            // even take generics, right arity, etc.) to Typecheck_Tree, which
            // already knows how to resolve a method's real symbol.
            if (s_curr.type == TOK_LBRACKET && !s_curr_newline_before) {
                LexerState ta_save;
                Lexer_Save(&ta_save);
                Token lb_tok = s_curr;
                advance(); // consume '['
                Type** targs = NULL;
                size_t tcount = 0, tcap = 0;
                bool ok = true;
                while (s_curr.type != TOK_RBRACKET) {
                    Type* ta = parse_type();
                    if (!ta) { ok = false; break; }
                    if (tcount >= tcap) {
                        tcap = tcap ? tcap * 2 : 4;
                        targs = (Type**)realloc(targs, tcap * sizeof(Type*));
                    }
                    targs[tcount++] = ta;
                    if (s_curr.type == TOK_COMMA) { advance(); continue; }
                    break;
                }
                if (ok && s_curr.type == TOK_RBRACKET) {
                    advance(); // consume ']'
                    if (s_curr.type == TOK_LPAREN) {
                        // Genuine explicit-generic method call: build the
                        // AST_CALL directly here (skipping the ordinary
                        // TOK_LPAREN branch below, which doesn't know about
                        // type_args) so type_args/type_arg_count land on the
                        // call node try_rewrite_method_call/infer_generic
                        // already read for a top-level generic call -- same
                        // fields, same downstream handling, no new machinery.
                        advance(); // consume '('
                        ASTNode* call_node = new_node(AST_CALL);
                        call_node->call.target_name = NULL;
                        call_node->call.target_expr = node; // the AST_FIELD
                        call_node->call.args = parse_call_arg_list(&call_node->call.arg_count);
                        call_node->call.type_args = targs;
                        call_node->call.type_arg_count = tcount;
                        node = call_node;
                        continue; // re-enter the while loop; this `[...]  (` is consumed
                    }
                }
                // Not `[TypeArgs](` after all (plain indexing, or malformed) --
                // rewind completely and let the ordinary TOK_LBRACKET branch
                // below parse it as indexing, unchanged.
                free(targs);
                Lexer_Restore(&ta_save);
                s_curr = lb_tok;
                s_curr_newline_before = false; // conservative, mirrors the cast-disambiguation rewind above
            }
        } else if (s_curr.type == TOK_LBRACKET) { // TOK_LBRACKET — indexing
            advance();
            ASTNode* idx = parse_expr_prec(0);
            if (s_curr.type != TOK_RBRACKET) parse_error("Expected ']' after index");
            advance();
            ASTNode* inode = new_node(AST_INDEX);
            inode->index.base = node;
            inode->index.index = idx;
            node = inode;
        } else if (s_curr.type == TOK_LPAREN) { // TOK_LPAREN — indirect call
            advance();
            ASTNode* call_node = new_node(AST_CALL);
            call_node->call.target_name = NULL;
            call_node->call.target_expr = node;
            call_node->call.args = parse_call_arg_list(&call_node->call.arg_count);
            node = call_node;
        }
    }
    return node;
}

static ASTNode* parse_expr_prec(int min_prec) {
    ASTNode* left = parse_postfix();
    
    while (1) {
        int prec = get_token_prec(s_curr.type);
        if (prec < min_prec) {
            break;
        }
        // A newline before a binary operator acts as an implicit statement terminator,
        // except for assignment operators (prec 1) which are always continuations.
        // This prevents `a = &x\n*p = 1` from being parsed as `a = (&x) * (p = 1)`.
        if (Lexer_NewlineBefore && prec > 1) {
            break;
        }

        TokenType op = s_curr.type;
        advance();

        // Assignment family is right-associative (= and all op=).
        int next_prec = (prec == 1) ? prec : prec + 1;

        ASTNode* right = parse_expr_prec(next_prec);

        // Compound assignment a op= b  desugars to  a = a <op> b.
        // The lvalue subtree (`left`) is shared between the assign target and the
        // binop's own left operand -- is_compound tells codegen so it can spill the
        // destination address once instead of walking a complex lvalue (a call in an
        // index/field base, a deref of a call, etc.) twice.
        ASTNodeType compound = AST_ASSIGN;
        bool is_compound = true;
        switch (op) {
            case TOK_PLUS_EQ:  compound = AST_ADD; break;
            case TOK_MINUS_EQ: compound = AST_SUB; break;
            case TOK_STAR_EQ:  compound = AST_MUL; break;
            case TOK_SLASH_EQ: compound = AST_DIV; break;
            case TOK_MOD_EQ:   compound = AST_MOD; break;
            case TOK_AMP_EQ:   compound = AST_BIT_AND; break;
            case TOK_PIPE_EQ:  compound = AST_BIT_OR; break;
            case TOK_CARET_EQ: compound = AST_BIT_XOR; break;
            case TOK_SHL_EQ:   compound = AST_SHL; break;
            case TOK_SHR_EQ:   compound = AST_SHR; break;
            default: is_compound = false; break;
        }

        if (is_compound) {
            ASTNode* binop = new_node(compound);
            binop->binary.left = left;   // shared with the assign target
            binop->binary.right = right;
            ASTNode* node = new_node(AST_ASSIGN);
            node->binary.left = left;
            node->binary.right = binop;
            node->binary.is_compound = true;
            left = node;
        } else {
            ASTNode* node = new_node(get_op_node_type(op));
            node->binary.left = left;
            node->binary.right = right;
            left = node;
        }
    }
    
    return left;
}

// `match` LOWERS at parse time to an if-chain of EXISTING nodes -- there is no
// AST_MATCH; the backend/typecheck/constexpr never see `match`, only the if-chain they
// already compile. So match works wherever an if-chain works (including, eventually,
// constexpr once it can hold an aggregate). Lowering of
//   match e { Disconnect r {A}  Connect c {B}  Empty {C} }
// is, inside a fresh block scope:
//   EnumType _m = e
//   if (*(u32*)&_m == 0) { vtype r = _m.Disconnect; A }
//   else if (*(u32*)&_m == 1) { vtype c = _m.Connect; B }
//   else if (*(u32*)&_m == 2) { C }                         // exhaustive -> last arm
// Tag read = *(u32*)&_m (existing cast+deref+addr). Payload read = _m.Variant (a
// variant IS a StructField -> existing field access). Exhaustiveness is checked here.
// Primitive-scrutinee path: match on an int/bool value directly. Arms are bare
// (possibly negative) literal expressions -- no variant name, no payload binding.
// Shares the same outer-block/synthetic-local shape as the enum path; only the
// per-arm condition (direct equality vs tag-read + field access) differs.


// Does this pattern match EVERY value of its type -- i.e. can it never reject?
// True for a bare bind (AST_IDENT), and recursively true for a struct/array
// destructure pattern where every sub-element also covers all. False for any
// literal (constrains to one concrete value) and for an enum-variant literal
// (constrains to one variant among possibly others). Used to decide whether a
// pattern fully covers a variant's payload for exhaustiveness purposes -- a
// `{a, b, c}` array-destructure that only binds is just as "catch-all" as a
// bare identifier would be, since neither can ever fail to match.
static bool pattern_covers_all(ASTNode* pat) {
    if (pat->type == AST_IDENT) return true;
    // [GENERALIZE-LEAF] An lvalue-access leaf (`*x`, `x[i]`, `x.f`) is a total
    // target -- assigning the projection into an existing place can never
    // reject, exactly like a fresh bind can't. So it "covers all" too.
    if (pat->type == AST_DEREF || pat->type == AST_INDEX || pat->type == AST_FIELD) return true;
    if (pat->type == AST_STRUCT_LITERAL && !pat->struct_lit.is_enum_variant) {
        for (size_t i = 0; i < pat->struct_lit.count; i++)
            if (!pattern_covers_all(pat->struct_lit.values[i])) return false;
        return true;
    }
    if (pat->type == AST_ARRAY_LITERAL) {
        for (size_t i = 0; i < pat->array_lit.count; i++)
            if (!pattern_covers_all(pat->array_lit.values[i])) return false;
        return true;
    }
    return false;
}

// Read an enum's tag out of `scrut`: *(u32*)&scrut. Both compile_pattern's
// `.Variant` case and the cursor for-in desugar build this same read to
// compare against a variant index.
static ASTNode* make_tag_read(ASTNode* scrut) {
    ASTNode* addr = new_node(AST_ADDR); addr->unary = scrut;
    Type* u32t = (Type*)calloc(1, sizeof(Type)); u32t->cls = TYPE_PRIMITIVE; u32t->primitive = PRIM_U32;
    Type* u32p = (Type*)calloc(1, sizeof(Type)); u32p->cls = TYPE_POINTER; u32p->pointer_base = u32t;
    ASTNode* c = new_node(AST_CAST); c->cast.target_type = u32p; c->cast.expr = addr;
    ASTNode* d = new_node(AST_DEREF); d->unary = c;
    return d;
}
// Build `tag(scrut) == idx`.
static ASTNode* make_tag_eq(ASTNode* scrut, int idx) {
    ASTNode* tag = make_tag_read(scrut);
    ASTNode* lit = new_node(AST_INT_LITERAL); lit->lit_kind = LIT_INT; lit->int_value = (uint64_t)idx;
    ASTNode* eq = new_node(AST_EQ); eq->binary.left = tag; eq->binary.right = lit;
    return eq;
}

static void compile_pattern(ASTNode* pat, ASTNode* scrut, Type* scrut_type, ASTNode** out_cond, ASTNode*** out_decls, size_t* decl_count, size_t* decl_cap) {
    if (pat->type == AST_IDENT) {
        // [GENERALIZE-LEAF] If the name ALREADY exists in scope, this is a
        // destructuring-ASSIGNMENT into it, not a re-declaration (which would be
        // an "already declared" error). Only a fresh name declares. This makes
        // `unpack { a, b } = t` swap into existing a,b, and lets a bare existing
        // var appear as a leaf alongside `*x`/`x[i]`/`x.f`.
        Symbol* existing = SymTable_Find(s_symtable, pat->ident.name, pat->ident.name_len);
        if (existing) {
            // A switch pre-declared binder is registered with a NULL placeholder
            // type (predeclare_binders). This is a fresh binding whose type we now
            // know, not a destructure-assign into a pre-existing variable: patch the
            // symbol's type and emit a real decl-init (matching the fresh-name path
            // below), so the arm body sees a properly typed local.
            if (existing->type == NULL) {
                existing->type = scrut_type;
                ASTNode* decl = make_decl_stmt(scrut_type, pat->ident.name, pat->ident.name_len, existing, scrut);
                DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
                return;
            }
            ASTNode* asn = new_node(AST_ASSIGN);
            pat->ident.sym = existing;
            asn->binary.left = pat;
            asn->binary.right = scrut;
            DA_PUSH(*out_decls, *decl_count, *decl_cap, asn);
            return;
        }
        SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
        Symbol* sym = SymTable_Add(s_symtable, pat->ident.name, pat->ident.name_len, scrut_type, kind);
        ASTNode* decl = make_decl_stmt(scrut_type, pat->ident.name, pat->ident.name_len, sym, scrut);
        DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
        return;
    }

    // [GENERALIZE-LEAF] A pattern leaf that is an LVALUE ACCESS (`*x`, `x[i]`,
    // `x.f`) rather than a fresh binder: instead of declaring a new name, ASSIGN
    // the projected scrutinee into that existing place. This is destructuring-
    // ASSIGNMENT (vs the AST_IDENT case's destructuring-DECLARATION). The leaf
    // expression is used verbatim as the assign target -- it's already a valid
    // lvalue by the same rules any `*x = e` / `a[i] = e` / `a.f = e` statement
    // uses, so no new lvalue concept is introduced. Emitted as an AST_ASSIGN
    // pushed onto the same decl list; the backend already lowers it.
    if (pat->type == AST_DEREF || pat->type == AST_INDEX || pat->type == AST_FIELD) {
        ASTNode* asn = new_node(AST_ASSIGN);
        asn->binary.left = pat;      // the lvalue access, verbatim
        asn->binary.right = scrut;   // the projected scrutinee value
        DA_PUSH(*out_decls, *decl_count, *decl_cap, asn);
        return;
    }
    
    // Enum-variant pattern: `.Variant(payload)` (is_enum_variant=true, field_names[0]
    // = variant name, values[0] = payload sub-pattern if count==1) OR the designated-
    // literal idiom `{.Variant = payload}` (is_enum_variant=false, but sdef resolved
    // to the enum itself with exactly one field -- same shape, different spelling,
    // see the identical broadening in the exhaustiveness check above). Emit a
    // tag-equality check ANDed into cond, then recurse into the payload sub-pattern
    // via a field access on the SAME scrut node (reused, same as every other branch
    // here -- scrut is always a side-effect-free chain of field/index/ident reads on
    // top of the one materialized match temp, so reusing the pointer in two places
    // just re-emits the same read twice).
    // MUST come before the plain AST_STRUCT_LITERAL branch below: that branch
    // matches any struct literal unconditionally, including enum-variant ones,
    // and would treat the variant name as a bogus field name -- worse, for a
    // no-payload variant its count==0 loop body never runs, so it returns
    // having added NO condition at all, silently making that arm match always.
    // Real bug found and fixed via this exact gap: `{.Some = v}` fell through to
    // the plain-struct branch, read `scrut.Some` unconditionally with no tag
    // check, and silently returned garbage/wrong values on a `.None`-tagged scrut
    // instead of failing to match.
    bool is_variant_pat = pat->type == AST_STRUCT_LITERAL &&
        (pat->struct_lit.is_enum_variant ||
         (scrut_type && scrut_type->cls == TYPE_STRUCT && pat->struct_lit.sdef &&
          pat->struct_lit.sdef->is_enum && pat->struct_lit.count == 1));
    if (is_variant_pat) {
        StructDef* sd = (scrut_type && scrut_type->cls == TYPE_STRUCT) ? Struct_Find(scrut_type->struct_name) : NULL;
        if (!sd || !sd->is_enum) parse_error("'.Variant' pattern used against a non-enum scrutinee");

        int vidx = Enum_VariantIndex(sd, pat->struct_lit.field_names[0], pat->struct_lit.field_name_lens[0]);
        if (vidx < 0) parse_error("match arm names a variant this enum does not have");

        ASTNode* eq = make_tag_eq(scrut, vidx);

        if (*out_cond == NULL) {
            *out_cond = eq;
        } else {
            ASTNode* and_node = new_node(AST_LOGICAL_AND);
            and_node->binary.left = *out_cond;
            and_node->binary.right = eq;
            *out_cond = and_node;
        }

        if (pat->struct_lit.count == 1) {
            Type* ptype = sd->fields[vidx].type;
            if (!ptype) parse_error("match arm binds a payload but this variant has none");
            ASTNode* payload_scrut = new_node(AST_FIELD);
            payload_scrut->field.base = scrut;
            payload_scrut->field.field_name = pat->struct_lit.field_names[0];
            payload_scrut->field.field_name_len = pat->struct_lit.field_name_lens[0];
            compile_pattern(pat->struct_lit.values[0], payload_scrut, ptype, out_cond, out_decls, decl_count, decl_cap);
        }
        return;
    }

    if (pat->type == AST_STRUCT_LITERAL) {
        for (size_t i = 0; i < pat->struct_lit.count; i++) {
            ASTNode* elem_pat = pat->struct_lit.values[i];
            ASTNode* elem_scrut = new_node(AST_FIELD);
            elem_scrut->field.base = scrut;
            elem_scrut->field.field_name = pat->struct_lit.field_names[i];
            elem_scrut->field.field_name_len = pat->struct_lit.field_name_lens[i];
            Type* elem_type = NULL;
            if (scrut_type && scrut_type->cls == TYPE_STRUCT) {
                StructDef* sd = Struct_Find(scrut_type->struct_name);
                if (sd) {
                    StructField* field = Struct_FindField(sd, pat->struct_lit.field_names[i], pat->struct_lit.field_name_lens[i]);
                    if (field) elem_type = field->type;
                }
            }
            compile_pattern(elem_pat, elem_scrut, elem_type, out_cond, out_decls, decl_count, decl_cap);
        }
        return;
    }

    if (pat->type == AST_ARRAY_LITERAL) {
        for (size_t i = 0; i < pat->array_lit.count; i++) {
            ASTNode* elem_pat = pat->array_lit.values[i];
            ASTNode* index = new_node(AST_INT_LITERAL);
            index->lit_kind = LIT_INT;
            index->int_value = i;
            ASTNode* elem_scrut = new_node(AST_INDEX);
            elem_scrut->index.base = scrut;
            elem_scrut->index.index = index;
            Type* elem_type = NULL;
            if (scrut_type && scrut_type->cls == TYPE_ARRAY) {
                elem_type = scrut_type->array.element;
            }
            compile_pattern(elem_pat, elem_scrut, elem_type, out_cond, out_decls, decl_count, decl_cap);
        }
        return;
    }
    
    ASTNode* eq = new_node(AST_EQ);
    eq->binary.left = scrut;
    eq->binary.right = pat;
    
    if (*out_cond == NULL) {
        *out_cond = eq;
    } else {
        ASTNode* and_node = new_node(AST_LOGICAL_AND);
        and_node->binary.left = *out_cond;
        and_node->binary.right = eq;
        *out_cond = and_node;
    }
}

// ─── shared match-arm if-chain skeleton ────────────────────────────────────
// parse_match_value (value scrutinee) and parse_match_type (type scrutinee)
// both lower their arms to the SAME shape: a block containing zero or more
// `if (cond) {A} else if (cond2) {B} else {C}`-style chains built one arm at
// a time, where each new arm either becomes the first statement in the
// block or gets threaded onto the previous arm's false_block. That threading
// (and the underlying growable statement list) used to be hand-copied in
// both functions, including an identical block-growth macro under two
// different names (PUSH / PUSH_T). It is pure tree shape -- neither
// function's arm-specific logic (value-pattern exhaustiveness vs.
// type-pattern wildcard scoping) needs to know how the chain is threaded --
// so it now lives once, here.
typedef struct {
    ASTNode* outer;         // the block the caller returns
    ASTNode* chain_tail_if; // innermost if whose false_block extends next; NULL = chain not started
} MatchChain;

static MatchChain matchchain_begin(void) {
    MatchChain mc;
    mc.outer = new_node(AST_BLOCK);
    mc.outer->block.capacity = 4;
    mc.outer->block.count = 0;
    mc.outer->block.statements = malloc(mc.outer->block.capacity * sizeof(ASTNode*));
    mc.chain_tail_if = NULL;
    return mc;
}

static void matchchain_push_stmt(MatchChain* mc, ASTNode* stmt) {
    append_block_statement(mc->outer, stmt);
}

// Attach one arm to the chain. `armblk` is the arm's already-parsed body.
// `ifn` is NULL for a terminal wildcard/else arm (armblk is linked directly);
// otherwise ifn is the AST_IF node the caller built for a conditional arm
// (with true_block == armblk), and this function threads it onto the chain.
// Returns false once a terminal (wildcard) arm has been attached, signaling
// the caller's loop to stop.
static bool matchchain_add_arm(MatchChain* mc, ASTNode* ifn, ASTNode* armblk, bool is_terminal) {
    ASTNode* to_link = is_terminal ? armblk : ifn;
    if (!mc->chain_tail_if) matchchain_push_stmt(mc, to_link);
    else mc->chain_tail_if->if_stmt.false_block = to_link;
    mc->chain_tail_if = is_terminal ? NULL : ifn;
    return !is_terminal;
}

// Lower an already-parsed value AST_MATCH into an if-chain, now that the scrutinee
// type `st` is known (typecheck time, generic params concrete). The arm bodies were
// already parsed into their own scopes by parse_match, so any pattern BINDINGS
// (enum payloads) are prepended to each arm body here via compile_pattern, whose
// decls slot in ahead of the already-parsed statements. Reuses compile_pattern +
// matchchain + the enum/bool exhaustiveness logic; the ONLY thing that moved is
// WHEN this runs (after Type_Infer, not during parsing), which is what lets a
// scrutinee like `N + 1` or `arr[N]` resolve at all.
ASTNode* Lower_Match(ASTNode* node, Type* st) {
    ASTNode* scrut = node->match_stmt.scrutinee;
    size_t narms = node->match_stmt.arm_count;
    uint64_t sz = Type_SizeOf(st); if (sz == 0) sz = 1;
    bool is_bool = (st->cls == TYPE_PRIMITIVE && st->primitive == PRIM_BOOL);

    SymbolTable* prev_table = s_symtable;
    s_symtable = SymTable_Create(prev_table);
    s_symtable->is_function_scope = prev_table->is_function_scope;
    SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;

    static int s_switch_ctr = 0;
    char* mname = malloc(32); int mn = snprintf(mname, 32, "$s%d", s_switch_ctr++);
    Symbol* msym = SymTable_Add(s_symtable, mname, mn, st, kind);

    MatchChain mc = matchchain_begin();
    ASTNode* outer = mc.outer;
    matchchain_push_stmt(&mc, make_decl_stmt(st, mname, mn, msym, scrut));
    #define SREF() ({ ASTNode* r = new_node(AST_IDENT); r->ident.name = mname; \
                      r->ident.name_len = mn; r->ident.sym = msym; r; })

    StructDef* enum_sd = (st->cls == TYPE_STRUCT) ? Struct_Find(st->struct_name) : NULL;
    bool is_enum_match = enum_sd && enum_sd->is_enum;
    bool* enum_covered = is_enum_match ? calloc(enum_sd->field_count, sizeof(bool)) : NULL;
    bool covered_true = false, covered_false = false, has_wildcard = false;
    uint8_t** seen = NULL; size_t seen_count = 0, seen_cap = 0; // folded constant arm values, for duplicate detection

    for (size_t a = 0; a < narms; a++) {
        ASTNode* pat  = node->match_stmt.arm_patterns[a];
        ASTNode* body = node->match_stmt.arm_bodies[a];
        SymbolTable* arm_scope = node->match_stmt.arm_scopes[a];
        bool is_wildcard = (pat == NULL);

        if (!is_wildcard && Type_IsAggregate(st)) resolve_brace_literal(pat, st);

        // Enum-variant exhaustiveness tracking (same rule as parse_match_value).
        bool is_variant_pattern = !is_wildcard && is_enum_match && pat->type == AST_STRUCT_LITERAL &&
            (pat->struct_lit.is_enum_variant ||
             (pat->struct_lit.sdef == enum_sd && pat->struct_lit.count == 1));
        if (is_variant_pattern) {
            int vidx = Enum_VariantIndex(enum_sd, pat->struct_lit.field_names[0], pat->struct_lit.field_name_lens[0]);
            if (vidx >= 0) {
                if (enum_covered[vidx]) parse_error("unreachable match arm: this variant is already fully covered by an earlier arm");
                bool covers_all = (pat->struct_lit.count == 0) ||
                                   (pat->struct_lit.count == 1 && pattern_covers_all(pat->struct_lit.values[0]));
                if (covers_all) enum_covered[vidx] = true;
            }
        }

        ASTNode* cond = NULL;
        if (!is_wildcard) {
            ASTNode** decls = NULL; size_t dc = 0, dcap = 0;
            // Compile the pattern IN THE ARM'S OWN SCOPE, so its binders resolve to
            // the exact symbols predeclare_binders registered (now getting their
            // real projected types via compile_pattern's placeholder-patch leaf).
            SymbolTable* save = s_symtable;
            s_symtable = arm_scope;
            compile_pattern(pat, SREF(), st, &cond, &decls, &dc, &dcap);
            s_symtable = save;
            // Prepend the pattern's binding decls ahead of the arm body's own statements.
            if (dc > 0) {
                ASTNode* merged = new_node(AST_BLOCK);
                merged->block.capacity = dc + body->block.count;
                merged->block.statements = malloc(merged->block.capacity * sizeof(ASTNode*));
                merged->block.count = 0;
                for (size_t i = 0; i < dc; i++) merged->block.statements[merged->block.count++] = decls[i];
                for (size_t i = 0; i < body->block.count; i++) merged->block.statements[merged->block.count++] = body->block.statements[i];
                body = merged;
            }
            if (decls) free(decls);
            // Duplicate-arm / literal-fit checks for constant-valued patterns (same
            // as parse_match_value): fold the pattern to bytes and compare against
            // earlier arms. Covers primitives and constant aggregate/enum literals.
            uint8_t* bytes = (uint8_t*)calloc(1, sz);
            if (ConstEval_Bytes(pat, bytes, sz)) {
                // Integer fit-check only for INTEGER scrutinees. Floats are allowed
                // as scrutinees (unlike the old `match`, which arbitrarily banned
                // them); an integer-range check is meaningless for a float literal.
                if (st->cls == TYPE_PRIMITIVE && !Type_IsFloat(st)) {
                    int64_t val = 0; ConstEval(pat, &val);
                    if (!Type_IntLiteralFits(val, st)) parse_error("match arm literal does not fit the scrutinee's type (use a cast to truncate, or widen the scrutinee)");
                }
                for (size_t i = 0; i < seen_count; i++)
                    if (memcmp(seen[i], bytes, sz) == 0) parse_error("duplicate match arm for this value");
                DA_PUSH(seen, seen_count, seen_cap, bytes);
                if (is_bool) { if (bytes[0]) covered_true = true; else covered_false = true; }
            } else {
                free(bytes);
            }
        } else {
            has_wildcard = true;
        }

        if (is_wildcard) { matchchain_add_arm(&mc, NULL, body, true); break; }
        if (!cond) { cond = new_node(AST_INT_LITERAL); cond->lit_kind = LIT_BOOL; cond->int_value = 1; }
        ASTNode* ifn = new_node(AST_IF);
        ifn->if_stmt.condition = cond; ifn->if_stmt.true_block = body; ifn->if_stmt.false_block = NULL;
        matchchain_add_arm(&mc, ifn, body, false);
    }

    if (!has_wildcard) {
        bool exhaustive = is_bool && covered_true && covered_false;
        if (is_enum_match) {
            exhaustive = true;
            for (size_t i = 0; i < enum_sd->field_count; i++) if (!enum_covered[i]) { exhaustive = false; break; }
        }
        if (!exhaustive) {
            s_symtable = prev_table;
            if (is_enum_match) parse_error("match on this enum is not exhaustive -- add an `else` arm or handle every variant");
            else if (st->cls == TYPE_PRIMITIVE && !is_bool) parse_error("match on this type is not exhaustive -- add an `else` arm (only bool can be exhaustive without one)");
            else parse_error("match on a struct/array/pointer/function value is not exhaustive -- add an `else` arm");
        }
        if (mc.chain_tail_if) mc.chain_tail_if->if_stmt.exhaustive_tail = true;
    }
    if (enum_covered) free(enum_covered);
    for (size_t i = 0; i < seen_count; i++) free(seen[i]);
    free(seen);
    #undef SREF
    s_symtable = prev_table;
    return outer;
}

// TYPE-scrutinee match: `match T { P* {..} E[N] {..} u32 {..} else {..} }`.
//
// This is reflection over a type's shape. It lowers, exactly like the enum path,
// to a chain of AST_IF nodes — but each arm is a TYPE PATTERN, not a tag read:
//
//   match T {
//       P*   { body_a }      ->  if <reflect P* against T>   { body_a }
//       E[N] { body_b }               else if <reflect E[N] against T> { body_b }
//       else { body_c }               else { body_c }
//   }
//
// Each arm's pattern is parsed with s_in_match_pattern = true, so an undeclared
// identifier becomes a fresh wildcard (parse_type_ex, above). The wildcard names
// are then published into the type-param scope while the arm BODY is parsed, so
// the body can reference P / E / N as ordinary types. The AST_IF carries the
// pattern and scrutinee type; branch selection (types.c AST_IF case) runs
// reflect_unify and, on a match, substitutes the bindings into the body. No new
// evaluator, no TypeInfo structs, no magic integers — the shape falls out of the
// existing if-chain + monomorphization machinery, matching Torrent's design rule.
static ASTNode* parse_match_type(ASTNode* scrut) {
    Type* scrut_type = scrut->sizeof_expr.type;

    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' to begin match arms");
    advance();

    MatchChain mc = matchchain_begin();
    ASTNode* outer = mc.outer;
    bool has_wildcard = false;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        bool is_else = (s_curr.type == TOK_ELSE);

        Type* pattern = NULL;
        // Snapshot the wildcard set for THIS arm (it's per-arm state).
        size_t wc_start = s_match_wildcard_count;

        if (is_else) {
            advance();
        } else {
            // Parse the pattern type in match-pattern mode: undeclared identifiers
            // become fresh wildcards, registered into s_match_wildcards[wc_start..].
            bool prev = s_in_match_pattern;
            s_in_match_pattern = true;
            bool prev_pt = s_pattern_types_ok; s_pattern_types_ok = true;
            pattern = parse_type();
            s_in_match_pattern = prev;
            s_pattern_types_ok = prev_pt;
            if (!pattern) parse_error("Expected a type pattern in match arm");
        }

        // Publish this arm's wildcards into the type-param scope so the BODY can
        // reference them (P, E as types; N as a value param). We extend the scope
        // arrays temporarily and restore them after the body. A size wildcard `N`
        // is a VALUE param (pinned u32); a bare `P`/`E` is a TYPE param (NULL pin).
        const char** prev_tp = s_type_params;
        Type** prev_pk = s_param_kinds;
        size_t prev_tpc = s_type_param_count;
        size_t nwc = s_match_wildcard_count - wc_start;
        const char** merged_names = NULL;
        Type** merged_kinds = NULL;
        if (nwc > 0) {
            size_t total = prev_tpc + nwc;
            merged_names = (const char**)malloc(total * sizeof(char*));
            merged_kinds = (Type**)malloc(total * sizeof(Type*));
            for (size_t i = 0; i < prev_tpc; i++) { merged_names[i] = prev_tp[i]; merged_kinds[i] = prev_pk ? prev_pk[i] : NULL; }
            for (size_t i = 0; i < nwc; i++) {
                merged_names[prev_tpc + i] = s_match_wildcards[wc_start + i];
                // A size wildcard (`N` in `E[N]`) is a VALUE param: pin it to u32 so
                // that when the body uses it as a value (`(i32)N`), it parses as an
                // AST_IDENT value param (not an AST_TYPE_EXPR) and clone_ast lowers
                // it from its bound TYPE_CONST_VALUE to a pinned literal — the exact
                // path const generics already use. A bare type wildcard (`P`, `E`)
                // stays a type param (NULL pin).
                merged_kinds[prev_tpc + i] =
                    (s_match_wc_is_size && s_match_wc_is_size[wc_start + i])
                        ? Type_MakePrim(PRIM_U32) : NULL;
            }
            s_type_params = merged_names;
            s_param_kinds = merged_kinds;
            s_type_param_count = total;
        }

        // Parse the arm body as a block in its own symbol scope.
        ASTNode* armblk = parse_block_body();

        // Restore the type-param scope.
        s_type_params = prev_tp;
        s_param_kinds = prev_pk;
        s_type_param_count = prev_tpc;

        // Retire this arm's wildcards. match_wildcard_lookup scans the live slice,
        // so trimming back to wc_start means the NEXT arm starts fresh — a wildcard
        // name reused across arms (`fn(A) A` in one arm, `fn(A) B` in another) is a
        // brand-new hole per arm, not a shared binding. Without this, arm 2's `A`
        // would alias arm 1's `A` and consistency checks would compare across arms.
        s_match_wildcard_count = wc_start;

        if (is_else) {
            has_wildcard = true;
            matchchain_add_arm(&mc, NULL, armblk, true);
            break;
        }

        // Build the arm as an AST_IF carrying the reflect pattern. The condition is
        // a placeholder literal (never actually evaluated as a runtime condition —
        // branch selection uses reflect_unify); it only needs to be a valid node so
        // ordinary tree walks don't trip on a NULL condition.
        ASTNode* placeholder = new_node(AST_INT_LITERAL);
        placeholder->lit_kind = LIT_BOOL; placeholder->int_value = 0;
        ASTNode* ifn = new_node(AST_IF);
        ifn->if_stmt.condition = placeholder;
        ifn->if_stmt.true_block = armblk;
        ifn->if_stmt.false_block = NULL;
        ifn->if_stmt.reflect_pattern = pattern;
        ifn->if_stmt.reflect_scrutinee = scrut_type;

        matchchain_add_arm(&mc, ifn, armblk, false);
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end match");
    advance();

    (void)has_wildcard; // exhaustiveness of type matches isn't checkable in general
    return outer;
}

// Shared scrutinee-type classification for `match` and `unpack`. Both are the
// same underlying operation (bind names via compile_pattern's pattern walk)
// at different refutability -- match allows patterns that can fail (needs
// exhaustiveness reasoning), unpack requires pattern_covers_all instead. That
// difference is real and stays in each caller; what was NOT a real difference
// was the legality check for which TYPE a pattern can be matched against at
// all -- previously duplicated as two independently hand-written boolean
// blocks that had drifted (match excluded floats for its own NaN/duplicate-
// arm reasons; unpack never had a reason to, and simply didn't). This is the
// one classification both entry points actually need.
//
// Pointers and function values were excluded here for years with no
// documented reason, and no downstream code depends on the exclusion:
// compile_pattern's AST_IDENT case is `SymTable_Add` + a read, and never
// inspects st->cls. A bare-identifier (identity) pattern binds any type.
// They're included now.
typedef struct {
    bool is_enum;
    bool is_primitive;
    bool is_aggregate;   // struct or array, and not an enum
    bool is_ptr_or_fn;   // pointer, function, or fn-literal
} ScrutKind;

static ScrutKind classify_scrutinee_type(Type* st) {
    ScrutKind k = {0};
    // A TYPE_PARAM is an unresolved generic placeholder (e.g. T) at template-
    // parse time.  Accept it now — the concrete type will be substituted by
    // clone_ast at instantiation, and Typecheck_Tree will re-validate then.
    if (st && st->cls == TYPE_PARAM) {
        k.is_primitive = true;
        return k;
    }
    k.is_enum = st && st->cls == TYPE_STRUCT && Struct_Find(st->struct_name)
                && Struct_Find(st->struct_name)->is_enum;
    k.is_primitive = st && st->cls == TYPE_PRIMITIVE;
    k.is_aggregate = Type_IsAggregate(st) && !k.is_enum;
    k.is_ptr_or_fn = st && (st->cls == TYPE_POINTER || st->cls == TYPE_FUNCTION || st->cls == TYPE_FN_LITERAL);
    return k;
}


// `unpack <pattern> = <expr>` -- sugar for a `match` whose pattern is known,
// at compile time, to always match: no `else` is possible or needed, and the
// names the pattern binds escape into the ENCLOSING scope (unlike a match arm,
// whose bindings die at the arm's closing brace). This reuses the exact same
// pattern grammar and compile_pattern walk that match's struct/array arms use
// -- `{.x=px, .y=py}`, `{px, py}` positional, nested, array `{a, b, c}`, and a
// bare identifier that binds the whole scrutinee with no peel at all -- so
// anything you could write as an irrefutable match arm, you can write here.
//
// Because there's no `else`, only patterns pattern_covers_all() accepts are
// allowed: no literal pins (`{.x=3, .y=py}`), no enum-variant patterns
// (`.Circle{r}`) -- those can fail at runtime and this form has nowhere to
// send the failure. Use `match` for anything refutable.
static ASTNode* parse_unpack(void) {
    advance(); // 'unpack'

    ASTNode* pat = parse_expr_prec(2); // above assignment (prec 1) -- stop before the '='

    if (s_curr.type != TOK_EQ) parse_error("Expected '=' after unpack pattern");
    advance(); // '='

    ASTNode* scrut = parse_expr_prec(0);
    // Inside a generic body, the scrutinee may reference the enclosing params (T)
    // that are still abstract here; publish them so a call like `inner(v)` (v:T)
    // can bind the callee's param to T -> inner[T]. Cleared right after.
    Types_SetEnclosingParams(s_type_params, s_type_param_count);
    Typecheck_Tree(scrut); // bottom-up generic inference for AST_CALL, same as match
    Types_SetEnclosingParams(NULL, 0);
    Type* st = Type_Infer(scrut);

    ScrutKind k = classify_scrutinee_type(st);
    if (k.is_enum) parse_error("unpack cannot bind an enum value -- an enum's payload is "
                                "refutable per-variant, so use `match` instead");

    if (!k.is_aggregate && !k.is_primitive && !k.is_ptr_or_fn)
        parse_error("unpack scrutinee must be a struct, array, primitive, pointer, or function value");

    // [GENERALIZE] Auto-deref a pointer-to-aggregate when the pattern is a
    // destructure (brace/array), not a bare bind. `unpack {..} = pp` then means
    // "descend through the pointer, then project" -- deref is just the first
    // access step, same story structs already tell for .field / [i]. A bare
    // `unpack w = pp` still binds the whole pointer (pat is AST_IDENT -> skipped).
    bool auto_deref = false;
    Type* proj_type = st;
    if (k.is_ptr_or_fn && st->cls == TYPE_POINTER && st->pointer_base &&
        pat->type != AST_IDENT) {
        ScrutKind pk = classify_scrutinee_type(st->pointer_base);
        if (pk.is_aggregate) {
            auto_deref = true;
            proj_type = st->pointer_base;
            k = pk;  // reclassify against the pointee
        }
    }

    // Positional `{a, b}` patterns need field names filled in the same way a
    // positional struct/array LITERAL does -- reuse the exact same pass.
    if (k.is_aggregate) resolve_brace_literal(pat, proj_type);

    if (!pattern_covers_all(pat))
        parse_error("unpack pattern must always match -- no literal-pinned fields "
                     "(e.g. `{.x=3, .y=py}`) or enum-variant patterns are allowed here; "
                     "use `match` if the pattern can fail");

    ASTNode* outer = new_node(AST_BLOCK);
    outer->block.capacity = 4; outer->block.count = 0;
    outer->block.statements = malloc(outer->block.capacity * sizeof(ASTNode*));
    // This wrapper only exists because parse_statement must return a single
    // node -- it is NOT a real scope. Bindings inside must survive past this
    // block's end (that's the whole point of unpack vs. match). The symbol
    // table already knows this (see the comment above), but ConstEval's
    // AST_BLOCK case scopes every block it sees unless told otherwise -- mark
    // this one transparent so comptime folding matches runtime semantics.
    outer->block.transparent = true;

    // Materialize the scrutinee into a temp first, exactly like match does --
    // so a scrutinee with side effects (a call, an index into something
    // mutating) is only evaluated once, no matter how many fields the pattern
    // reads out of it.
    SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
    static int s_unpack_ctr = 0;
    char* mname = malloc(32); int mn = snprintf(mname, 32, "$u%d", s_unpack_ctr++);
    Symbol* msym = SymTable_Add(s_symtable, mname, mn, st, kind);

    ASTNode* mdecl = make_decl_stmt(st, mname, mn, msym, scrut);
    append_block_statement(outer, mdecl);

    ASTNode* mref = new_node(AST_IDENT);
    mref->ident.name = mname; mref->ident.name_len = mn; mref->ident.sym = msym;

    // [GENERALIZE] If we auto-deref'd, the projection scrutinee is *mref, and
    // the type handed to compile_pattern is the pointee type.
    ASTNode* proj_scrut = mref;
    if (auto_deref) {
        ASTNode* d = new_node(AST_DEREF); d->unary = mref;
        proj_scrut = d;
    }

    // No child scope here (unlike match arms): compile_pattern's SymTable_Add
    // calls land directly in s_symtable, i.e. the scope unpack was written in,
    // so the bound names are visible to every statement after this one.
    ASTNode* cond = NULL;
    ASTNode** decls = NULL;
    size_t decl_count = 0, decl_cap = 0;
    compile_pattern(pat, proj_scrut, proj_type, &cond, &decls, &decl_count, &decl_cap);
    // cond is always NULL here: pattern_covers_all rejected anything that would
    // set it (literals, enum variants). Nothing to check at runtime.

    for (size_t i = 0; i < decl_count; i++) append_block_statement(outer, decls[i]);
    if (decls) free(decls);

    if (s_curr.type == TOK_SEMI) advance(); // optional separator

    return outer;
}

// Pre-declare a pattern's binder NAMES into the current scope at parse time, so an
// arm body referencing them (`{.Some = v} { return v }`) parses without knowing the
// scrutinee's type yet. Types are placeholders here; Lower_Match patches each
// symbol's real type once st is known, and compile_pattern's "name already in
// scope -> assign into it" leaf (GENERALIZE-LEAF) reuses these exact symbols. Only
// fresh identifiers in binding position declare; literals / existing lvalues don't.
static void predeclare_binders(ASTNode* pat) {
    if (!pat) return;
    if (pat->type == AST_IDENT) {
        // A bare name that isn't already in scope is a fresh binder. (An existing
        // name is a destructure-assignment target, not a new declaration.)
        if (!SymTable_Find(s_symtable, pat->ident.name, pat->ident.name_len)) {
            SymbolKind k = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
            SymTable_Add(s_symtable, pat->ident.name, pat->ident.name_len, NULL, k);
        }
        return;
    }
    if (pat->type == AST_STRUCT_LITERAL) {
        for (size_t i = 0; i < pat->struct_lit.count; i++) predeclare_binders(pat->struct_lit.values[i]);
        return;
    }
    if (pat->type == AST_ARRAY_LITERAL) {
        for (size_t i = 0; i < pat->array_lit.count; i++) predeclare_binders(pat->array_lit.values[i]);
        return;
    }
    // literals, .Variant heads with no payload, lvalue accesses: nothing to declare.
}

// ── `match` ──────────────────────────────────────────────────────────────────
// A previous value-match implementation resolved the scrutinee's TYPE at parse
// time (to pick enum/primitive/aggregate lowering), which broke whenever the
// scrutinee mentioned a still-abstract generic param (`match N + 1`, `match arr[N]`)
// and forced a series of parse-time special-cases. This implementation instead
// captures the scrutinee expression and the raw arm patterns/bodies into an
// AST_MATCH node WITHOUT any Type_Infer, and defers all classification + lowering to
// Lower_Match, which runs at typecheck time when generic params are concrete. The
// only decision kept at parse time is the irreducible one: value-scrutinee vs
// type-scrutinee, since their arm grammars differ. The type-scrutinee case delegates
// to parse_match_type (which already resolves correctly at instantiation, via
// reflect_unify); only the value case is captured as AST_MATCH.
//
// Because parse time makes NO type query on the value path, there is no parse-time
// resolution that can fail against an abstract param -- the bug class that motivated
// this rewrite is structurally absent, not merely handled.
static ASTNode* parse_match(void) {
    advance(); // 'match'

    // Grouped-type scrutinee probe: `match (fn() T)[4] {...}`.
    if (s_curr.type == TOK_LPAREN) {
        LexerState msave; Lexer_Save(&msave);
        Token cur_save = s_curr;
        Type* tt = parse_type();
        if (tt && s_curr.type == TOK_LBRACE) {
            ASTNode* te = new_node(AST_TYPE_EXPR);
            te->sizeof_expr.type = tt;
            return parse_match_type(te); // type-match already resolves at instantiation
        }
        Lexer_Restore(&msave); s_curr = cur_save;
    }

    ASTNode* scrut = parse_expr_prec(0);

    // A bare-type scrutinee (`match T`, `match Foo*`) parses as an AST_TYPE_EXPR:
    // type reflection, handled by parse_match_type.
    if (scrut->type == AST_TYPE_EXPR) return parse_match_type(scrut);

    // Value scrutinee: capture arms raw, NO scrutinee typechecking here.
    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' to begin match arms");
    advance();

    ASTNode* node = new_node(AST_MATCH);
    node->match_stmt.scrutinee = scrut;
    node->match_stmt.is_type_match = false;
    ASTNode** pats = NULL; size_t np = 0, pcap = 0;
    ASTNode** bodies = NULL; size_t nb = 0, bcap = 0;
    SymbolTable** scopes = NULL; size_t ns = 0, scap = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        ASTNode* pat = NULL; // NULL => else arm
        if (s_curr.type == TOK_ELSE) {
            advance();
        } else if (s_curr.type == TOK_DOT) {
            advance(); // '.'
            pat = parse_enum_variant_after_dot();
        } else {
            pat = parse_expr_prec(0);
        }

        // Each arm body parses in its OWN scope. Pre-declare the pattern's binders
        // (name-only; types patched by Lower_Match) so `{.Some = v} { return v }`
        // resolves. Stash the scope so Lower_Match reuses the same binder symbols.
        if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' for match arm block");
        SymbolTable* prev = s_symtable;
        s_symtable = SymTable_Create(prev);
        s_symtable->is_function_scope = prev->is_function_scope;
        if (pat) predeclare_binders(pat);
        SymbolTable* arm_scope = s_symtable;
        ASTNode* body = parse_block_body();
        s_symtable = prev;

        DA_PUSH(pats, np, pcap, pat);
        DA_PUSH(bodies, nb, bcap, body);
        DA_PUSH(scopes, ns, scap, arm_scope);

        if (!pat) break; // else is terminal
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end match");
    advance();

    node->match_stmt.arm_patterns = pats;
    node->match_stmt.arm_bodies = bodies;
    node->match_stmt.arm_scopes = scopes;
    node->match_stmt.arm_count = np;
    return node;
}

static ASTNode* parse_while_statement(void) {
    advance();
    ASTNode* condition = parse_expr_prec(0);
    s_loop_depth++;
    ASTNode* body = parse_block_body();
    s_loop_depth--;
    ASTNode* node = new_node(AST_WHILE);
    node->while_stmt.condition = condition;
    node->while_stmt.body = body;
    return node;
}

static ASTNode* parse_forin_statement(void); // fwd

// [FOR-IN] Two forms, both requiring an explicit leading keyword/type:
//   for TYPE name in ITERABLE { body }        -- single typed binding
//   for unpack PATTERN in ITERABLE { body }   -- explicit destructure
// Both desugar to a counting loop over 0..len(ITERABLE), binding at the top
// of each body iteration from ITERABLE[idx]. The TYPE form produces one
// plain typed declaration, exactly like the counting `for`'s own `TYPE i`.
// The `unpack` form runs the *same* compile_pattern walk the standalone
// `unpack` statement uses, so `{.x=px,.y=py}`, `{a,b}` positional, nested,
// and array patterns all work here identically to `unpack ... = expr`.
// There is no bare/untyped form and no un-keyworded brace form: `for x in
// arr` and `for {a,b} in arr` are both parse errors.
// ITERABLE is anything indexable: a built-in array, or an op-overloaded
// container exposing `__index` + a `len()` method. The element fetch is
// `ITERABLE[idx]`, which already dispatches through __index for overloaded
// containers, so overloading composes for free.

// A method resolved through its receiver's generic base (Method_Mangle) still
// has that base's own declared type as its return type. If the receiver is a
// concrete instantiation, substitute its args into the return type so the
// result names the actual instantiated type instead of the base's bare
// type-param symbol. This used to be a hand-rolled copy of Type_Substitute's
// own generic_base/type_arg_count guard living only here; it's now
// Type_Substitute_Through_Instance (types.c), the same shared helper
// types.c's own __assign-operand case uses -- one substitution primitive
// instead of two independent copies of it drifting apart across files.
static ASTNode* parse_forin_statement(void) {
    advance(); // past 'for'

    bool is_unpack_form = (s_curr.type == TOK_UNPACK);
    Type* decl_type = NULL;
    Token ivar = {0};
    ASTNode* pat = NULL;

    if (is_unpack_form) {
        advance(); // past 'unpack'
        pat = parse_expr_prec(2); // pattern grammar, same as standalone unpack
    } else {
        // TYPE name -- mandatory, same grammar as any other declaration.
        if (!curr_begins_type())
            parse_error("Expected 'unpack' or a loop variable type after 'for' "
                        "(e.g. `for u32 x in arr` or `for unpack {a,b} in arr`)");
        decl_type = parse_type();
        if (!decl_type) parse_error("Expected loop variable type after 'for'");
        if (s_curr.type != TOK_IDENTIFIER)
            parse_error("Expected loop variable name after type in 'for-in'");
        ivar = s_curr; advance();
    }

    // 'in'
    if (!(s_curr.type == TOK_IDENTIFIER && s_curr.length == 2 &&
          strncmp(s_curr.start, "in", 2) == 0))
        parse_error("Expected 'in' after for-in loop variable/pattern");
    advance(); // past 'in'

    // The ITERABLE expression.
    ASTNode* iter = parse_expr_prec(0);
    Typecheck_Tree(iter);
    Type* itype = Type_Infer(iter);


    // [CURSOR] O(1)-per-step protocol: iterable exposes `begin() Cursor`, and the
    // returned Cursor exposes `next() Option[Elem]` where Option is any 2-variant
    // enum with exactly one payload-carrying variant (the "some") and one
    // payloadless variant (the "none"). We detect this by name
    // (`begin`, `next`) but classify the Option variants structurally, so the user
    // can call their enum anything as long as its shape is {payload Some, None}.
    bool is_cursor = false;
    Type* cur_type = NULL;          // Cursor struct type (begin's return)
    Type* opt_type = NULL;          // Option enum instance (next's return)
    StructDef* opt_sd = NULL;       // its StructDef (is_enum)
    int some_idx = -1, none_idx = -1;
    Symbol* begin_fn = NULL;
    Type* elem_type = NULL;
    StructDef* csd = NULL;
    if (itype && itype->cls == TYPE_STRUCT) {
        csd = Struct_Find(itype->struct_name);
        // Cursor protocol takes priority when begin()->Cursor->next()->Option resolves.
        // Use Method_Mangle (not a raw "%s_begin"-on-struct_name snprintf) so this
        // resolves through generic_base for instantiated generics -- Vector[i32]'s
        // `begin` lives under the mangled name `Vector_begin`, not `Vector_i32_begin`.
        size_t bn = 0;
        char* bmangled = Method_Mangle(itype, "begin", 5, &bn);
        begin_fn = bmangled ? SymTable_Find(s_symtable, bmangled, bn) : NULL;
        free(bmangled);
        if (begin_fn && begin_fn->type && begin_fn->type->cls == TYPE_FUNCTION) {
            // A method found through the GENERIC BASE (comment above) has its
            // return type still in terms of THAT struct's own type params (e.g.
            // `Cur[T, N]` verbatim), even when its receiver was a concrete
            // instantiation. Substitute using the receiver's actual args so a
            // generic Cursor (and its generic next()) resolve to concrete types
            // instead of the bare param symbol -- else the loop var typechecks
            // as `T` itself. Applied once for begin()'s receiver (itype/csd) and
            // again for next()'s receiver (cur_type, once known).
            cur_type = Type_Substitute_Through_Instance(begin_fn->type->function.return_type, csd);
            if (cur_type && cur_type->cls == TYPE_STRUCT) {
                size_t nn = 0;
                char* nmangled = Method_Mangle(cur_type, "next", 4, &nn);
                Symbol* next_fn = nmangled ? SymTable_Find(s_symtable, nmangled, nn) : NULL;
                free(nmangled);
                if (next_fn && next_fn->type && next_fn->type->cls == TYPE_FUNCTION) {
                    StructDef* cur_sd_concrete = Struct_Find(cur_type->struct_name);
                    opt_type = Type_Substitute_Through_Instance(next_fn->type->function.return_type, cur_sd_concrete);
                    opt_sd = (opt_type && opt_type->cls == TYPE_STRUCT)
                                 ? Struct_Find(opt_type->struct_name) : NULL;
                    if (opt_sd && opt_sd->is_enum && opt_sd->field_count == 2) {
                        // classify: the payload-carrying variant is "some", the
                        // payloadless one is "none". Exactly one of each required.
                        for (size_t vi = 0; vi < 2; vi++) {
                            if (opt_sd->fields[vi].type) { if (some_idx < 0) some_idx = (int)vi; }
                            else                          { if (none_idx < 0) none_idx = (int)vi; }
                        }
                        if (some_idx >= 0 && none_idx >= 0 && some_idx != none_idx) {
                            Type* pay = opt_sd->fields[some_idx].type;
                            // Writable-leaf convention, same as __index: a pointer
                            // payload binds the loop var to the pointee.
                            elem_type = (pay && pay->cls == TYPE_POINTER) ? pay->pointer_base : pay;
                            is_cursor = true;
                        }
                    }
                }
            }
        }
    }
    if (!is_cursor)
        parse_error("for-in iterable must be a struct exposing `begin()` returning a "
                    "cursor with `next()` returning a 2-variant Option enum");

    // [CURSOR DESUGAR] Distinct lowering:
    //   { $it = ITERABLE; $cur = $it.begin();
    //     while true {
    //         Opt $o = $cur.next();
    //         if tag($o) == none_idx { break }
    //         <bind loopvar/pattern from $o.<some>>   // auto-deref if payload is ptr
    //         <user body>
    //     } }
    // Wrapped in an outer transparent block so $it/$cur/$o stay loop-local.
    SymbolTable* prev_table = s_symtable;
    s_symtable = SymTable_Create(prev_table);
    s_symtable->is_function_scope = prev_table->is_function_scope;
    SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
    static int s_cur_ctr = 0; int id = s_cur_ctr++;

    char* itname = malloc(32); int itlen = snprintf(itname, 32, "$ci_it%d", id);
    Symbol* itsym = SymTable_Add(s_symtable, itname, itlen, itype, kind);
    char* cname = malloc(32); int clen = snprintf(cname, 32, "$ci_cur%d", id);
    Symbol* csym = SymTable_Add(s_symtable, cname, clen, cur_type, kind);

    // $it = ITERABLE
    ASTNode* it_decl = make_decl_stmt(itype, itname, itlen, itsym, iter);
    // $cur = $it.begin()
    ASTNode* itref = make_ident_node(itname, itlen, itsym);
    ASTNode* bfield = new_node(AST_FIELD);
    bfield->field.base = itref;
    bfield->field.field_name = "begin"; bfield->field.field_name_len = 5;
    ASTNode* bcall = new_node(AST_CALL);
    bcall->call.target_name = NULL; bcall->call.target_expr = bfield;
    bcall->call.args = NULL; bcall->call.arg_count = 0;
    ASTNode* cur_decl = make_decl_stmt(cur_type, cname, clen, csym, bcall);

    // ---- loop body ----
    s_loop_depth++;
    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' for for-in body");
    SymbolTable* body_prev = s_symtable;
    s_symtable = SymTable_Create(body_prev);
    s_symtable->is_function_scope = body_prev->is_function_scope;
    SymbolKind bkind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;

    // Opt $o = $cur.next()
    char* oname = malloc(32); int olen = snprintf(oname, 32, "$ci_o%d", id);
    Symbol* osym = SymTable_Add(s_symtable, oname, olen, opt_type, bkind);
    ASTNode* curref = make_ident_node(cname, clen, csym);
    ASTNode* nfield = new_node(AST_FIELD);
    nfield->field.base = curref;
    nfield->field.field_name = "next"; nfield->field.field_name_len = 4;
    ASTNode* ncall = new_node(AST_CALL);
    ncall->call.target_name = NULL; ncall->call.target_expr = nfield;
    ncall->call.args = NULL; ncall->call.arg_count = 0;
    ASTNode* o_decl = make_decl_stmt(opt_type, oname, olen, osym, ncall);

    // if tag($o) == none_idx { break }
    ASTNode* oref = make_ident_node(oname, olen, osym);
    ASTNode* eqn = make_tag_eq(oref, none_idx);
    ASTNode* brk = new_node(AST_BREAK);
    ASTNode* brk_blk = new_node(AST_BLOCK);
    brk_blk->block.capacity = 1; brk_blk->block.count = 1;
    brk_blk->block.statements = malloc(sizeof(ASTNode*));
    brk_blk->block.statements[0] = brk;
    ASTNode* if_none = new_node(AST_IF);
    if_none->if_stmt.condition = eqn;
    if_none->if_stmt.true_block = brk_blk;
    if_none->if_stmt.false_block = NULL;

    // payload access: $o.<some-variant-name>  (an enum field read, same shape
    // the match payload branch produces). If payload was a pointer, deref it
    // so the loop var binds the pointee (writable leaf).
    ASTNode* oref2 = make_ident_node(oname, olen, osym);
    ASTNode* pfield = new_node(AST_FIELD);
    pfield->field.base = oref2;
    pfield->field.field_name = opt_sd->fields[some_idx].name;
    pfield->field.field_name_len = strlen(opt_sd->fields[some_idx].name);
    Type* pay = opt_sd->fields[some_idx].type;
    ASTNode* elem_access = pfield;
    // Writable-leaf / manual-reference rule: the payload is `T*`. If the user
    // declared the loop var as `T` (the pointee), auto-deref so they bind a
    // copy of the value. If they declared it as `T*` (matching the payload),
    // bind the pointer as-is -- assignment copies the pointer, and `*p = ...`
    // in the body writes through to the real element. No special write-back
    // machinery: manual referencing is the idiom, and it just falls out.
    // (Only the TYPE form can spell a pointer; the unpack form always derefs
    // to destructure the pointee aggregate.)
    bool user_wants_ptr = (!is_unpack_form && decl_type &&
                           pay && pay->cls == TYPE_POINTER &&
                           Type_Equals(decl_type, pay));
    if (pay && pay->cls == TYPE_POINTER && !user_wants_ptr) {
        ASTNode* d = new_node(AST_DEREF); d->unary = pfield; elem_access = d;
    }

    // bind: TYPE form -> one decl; unpack form -> compile_pattern walk
    ASTNode** bind_decls = NULL; size_t bind_count = 0;
    if (is_unpack_form) {
        if (Type_IsAggregate(elem_type)) resolve_brace_literal(pat, elem_type);
        if (!pattern_covers_all(pat))
            parse_error("for-in unpack pattern must always match");
        ASTNode* cond2 = NULL; ASTNode** pdecls = NULL; size_t pdc = 0, pdcap = 0;
        compile_pattern(pat, elem_access, elem_type, &cond2, &pdecls, &pdc, &pdcap);
        bind_decls = pdecls; bind_count = pdc;
    } else {
        Symbol* ivsym = SymTable_Add(s_symtable, ivar.start, ivar.length, decl_type, bkind);
        ASTNode* ivdecl = make_decl_stmt(decl_type, ivar.start, ivar.length, ivsym, elem_access);
        Typecheck_Tree(ivdecl);
        bind_decls = malloc(sizeof(ASTNode*)); bind_decls[0] = ivdecl; bind_count = 1;
    }

    ASTNode* user_body = parse_block_body();
    s_loop_depth--;

    // while-body block = [ $o decl, if-none-break, bind..., user stmts ]
    ASTNode* wbody = new_node(AST_BLOCK);
    size_t total = 2 + bind_count + user_body->block.count;
    wbody->block.capacity = total; wbody->block.count = 0;
    wbody->block.statements = malloc(total * sizeof(ASTNode*));
    wbody->block.statements[wbody->block.count++] = o_decl;
    wbody->block.statements[wbody->block.count++] = if_none;
    for (size_t i = 0; i < bind_count; i++) wbody->block.statements[wbody->block.count++] = bind_decls[i];
    for (size_t i = 0; i < user_body->block.count; i++)
        wbody->block.statements[wbody->block.count++] = user_body->block.statements[i];
    if (bind_decls) free(bind_decls);

    ASTNode* tru = make_int_literal(1, LIT_BOOL);
    ASTNode* wloop = new_node(AST_WHILE);
    wloop->while_stmt.condition = tru;
    wloop->while_stmt.body = wbody;

    s_symtable = prev_table;

    // outer transparent block holding $it/$cur + the while loop
    ASTNode* outer = new_node(AST_BLOCK);
    outer->block.capacity = 3; outer->block.count = 0;
    outer->block.statements = malloc(3 * sizeof(ASTNode*));
    outer->block.transparent = true;
    outer->block.statements[outer->block.count++] = it_decl;
    outer->block.statements[outer->block.count++] = cur_decl;
    outer->block.statements[outer->block.count++] = wloop;
    if (s_curr.type == TOK_SEMI) advance();
    return outer;


}

static ASTNode* parse_for_statement(void) {
    // [FOR-IN] Two, and only two, legal for-in header spellings, both
    // requiring an explicit keyword up front so neither is ever silently
    // inferred -- matching C++'s rule that the binding slot in a range-for
    // always names something (a type, or `auto`), never bare:
    //   for TYPE name in EXPR { ... }        -- single typed binding
    //   for unpack PATTERN in EXPR { ... }   -- explicit destructure
    // A bare `for x in arr` or an un-keyworded `for {a,b} in arr` are both
    // parse errors -- there is no implicit/untyped for-in form.
    //
    // Disambiguate from the counting `for TYPE i = A to B`, which is also
    // type-first: speculatively parse `TYPE name` and peek for `in` (for-in)
    // vs `=` (counting-for). `unpack` as the very next token after `for` is
    // unambiguous on its own -- counting-for can never start with `unpack`.
    {
        LexerState save; Lexer_Save(&save); Token save_cur = s_curr;
        advance(); // past 'for', to look at what follows
        bool is_forin = false;
        if (s_curr.type == TOK_UNPACK) {
            is_forin = true;
        } else if (curr_begins_type()) {
            parse_type(); // consumes the type; result unused, this is a peek
            if (s_curr.type == TOK_IDENTIFIER) {
                advance(); // consume the name
                if (s_curr.type == TOK_IDENTIFIER && s_curr.length == 2 &&
                    strncmp(s_curr.start, "in", 2) == 0) {
                    is_forin = true;
                }
            }
        }
        Lexer_Restore(&save); s_curr = save_cur;
        if (is_forin) return parse_forin_statement();
    }
    // Range for: `for TYPE i = A to B [by S] { body }`, exclusive end.
    //   init: TYPE i = A
    //   cond: i < B  (S>=0)  or  i > B  (S<0)
    //   incr: i = i + S
    // The loop variable is TYPE-FIRST like every other binding in Torrent
    // (var decls, params, fields) -- this is the SAME `parse_type() IDENT`
    // grammar, not a special untyped form. The range model is integer-or-float
    // counting: pointers are rejected because `p + step` silently scales by
    // element size (a footgun); walk a pointer with an explicit `while` instead.
    // Built as a real AST_FOR so the backend can land `continue` on the incr
    // (the for-as-while desugar would skip incr on continue -> infinite loop).
    advance();
    Type* ivtype = parse_type();
    if (!ivtype) parse_error("Expected loop variable type after 'for' (e.g. `for u32 i = 0 to 10`)");
    // Allowlist: integers + floats only. The range model needs `<` and `+step`;
    // pointers scale by element size (footgun) and bool/void aren't orderable counters.
    bool ok_ivtype = false;
    if (ivtype->cls == TYPE_PRIMITIVE) {
        switch (ivtype->primitive) {
            case PRIM_U8: case PRIM_U16: case PRIM_U32: case PRIM_U64:
            case PRIM_I8: case PRIM_I16: case PRIM_I32: case PRIM_I64:
            case PRIM_F32: case PRIM_F64:
                ok_ivtype = true; break;
            default: break; // bool, void/v rejected
        }
    }
    if (!ok_ivtype)
        parse_error("for loop variable must be an integer or float type (not pointer/bool/struct/array)");
    if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected loop variable name after type in 'for'");
    Token ivar = s_curr; advance();
    if (s_curr.type != TOK_EQ) parse_error("Expected '=' in for range");
    advance();
    ASTNode* start = parse_expr_prec(0);
    if (s_curr.type != TOK_TO) parse_error("Expected 'to' in for range");
    advance();
    ASTNode* end = parse_expr_prec(0);

    // Optional `by S` step (default +1). Direction taken from S's sign; when S is
    // a constexpr we pick the comparison at compile time (the common case).
    ASTNode* step = NULL;
    int64_t step_const = 1;
    bool step_is_const = true;
    if (s_curr.type == TOK_IDENTIFIER && s_curr.length == 2 &&
        strncmp(s_curr.start, "by", 2) == 0) {
        advance();
        step = parse_expr_prec(0);
        step_is_const = ConstEval(step, &step_const);
        if (step_is_const && step_const == 0) parse_error("for step cannot be zero");
    } else {
        // Default step +1. A plain int literal works for both int and float loop
        // vars now that mixed int/float arithmetic promotes the int operand to
        // float in codegen (`x = x + 1` on an f64 x correctly computes x+1.0).
        step = new_node(AST_INT_LITERAL);
        step->lit_kind = LIT_INT; step->int_value = 1;
    }

    SymbolTable* prev_table = s_symtable;
    s_symtable = SymTable_Create(prev_table);
    s_symtable->is_function_scope = prev_table->is_function_scope;

    // ivtype was parsed explicitly above (type-first), no inference needed.
    SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
    Symbol* isym = SymTable_Add(s_symtable, ivar.start, ivar.length, ivtype, kind);

    #define IREF() ({ ASTNode* r = new_node(AST_IDENT); r->ident.name = ivar.start; \
                      r->ident.name_len = ivar.length; r->ident.sym = isym; r; })

    // specs.md §9 promises the bound and step are each evaluated ONCE, at loop
    // entry -- not re-evaluated every iteration. Splicing `end`/`step` directly
    // into `cond`/`incr` (as this used to do) breaks that promise: those
    // subtrees are re-walked every iteration, so `for i32 i = 0 to limit()`
    // called limit() once per iteration instead of once total. Fix: hoist both
    // into hidden locals, evaluated once in the init block, and have
    // cond/incr reference those locals instead of the raw expressions.
    static int s_for_hoist_ctr = 0;
    char* end_name = malloc(32);
    int end_name_len = snprintf(end_name, 32, "$for_end%d", s_for_hoist_ctr);
    char* step_name = malloc(32);
    int step_name_len = snprintf(step_name, 32, "$for_step%d", s_for_hoist_ctr);
    s_for_hoist_ctr++;

    Symbol* end_sym = SymTable_Add(s_symtable, end_name, end_name_len, ivtype, kind);
    Symbol* step_sym = SymTable_Add(s_symtable, step_name, step_name_len, ivtype, kind);

    #define EREF() ({ ASTNode* r = new_node(AST_IDENT); r->ident.name = end_name; \
                      r->ident.name_len = end_name_len; r->ident.sym = end_sym; r; })
    #define SREF() ({ ASTNode* r = new_node(AST_IDENT); r->ident.name = step_name; \
                      r->ident.name_len = step_name_len; r->ident.sym = step_sym; r; })

    ASTNode* ivar_decl = make_decl_stmt(ivtype, ivar.start, ivar.length, isym, start);
    ASTNode* end_decl = make_decl_stmt(ivtype, end_name, end_name_len, end_sym, end);
    ASTNode* step_decl = make_decl_stmt(ivtype, step_name, step_name_len, step_sym, step);

    // This wrapper only exists because for_stmt.init is a single node slot --
    // it is NOT a real scope (ivar/end/step must be visible to cond/incr/body,
    // which sit outside this block). Mark it transparent (the same flag
    // unpack's synthetic wrapper uses) so ConstEval doesn't scope-unwind these
    // hoisted locals when it finishes walking the block.
    ASTNode* init = new_node(AST_BLOCK);
    init->block.capacity = 3; init->block.count = 0;
    init->block.statements = malloc(init->block.capacity * sizeof(ASTNode*));
    init->block.transparent = true;
    init->block.statements[init->block.count++] = ivar_decl;
    init->block.statements[init->block.count++] = end_decl;
    init->block.statements[init->block.count++] = step_decl;

    // cond: i < end for upward, i > end for downward. If step isn't constexpr,
    // direction can't be known at compile time -- build a runtime sign dispatch
    // instead of silently guessing (the guess was always `<`, which made a
    // non-foldable descending step run zero iterations: `i < end` is false
    // immediately when counting down from a start already >= end).
    //   (step < 0 && i > end) || (step >= 0 && i < end)
    // && and || both short-circuit, so exactly one comparison runs per
    // iteration -- same cost as the old compile-time-picked single comparison.
    ASTNode* cond;
    if (step_is_const) {
        cond = new_node(step_const < 0 ? AST_GT : AST_LT);
        cond->binary.left = IREF();
        cond->binary.right = EREF();
    } else {
        ASTNode* zero_lit = new_node(AST_INT_LITERAL);
        zero_lit->lit_kind = LIT_INT; zero_lit->int_value = 0;
        ASTNode* zero_lit2 = new_node(AST_INT_LITERAL);
        zero_lit2->lit_kind = LIT_INT; zero_lit2->int_value = 0;

        ASTNode* step_neg = new_node(AST_LT);
        step_neg->binary.left = SREF(); step_neg->binary.right = zero_lit;
        ASTNode* i_gt_end = new_node(AST_GT);
        i_gt_end->binary.left = IREF(); i_gt_end->binary.right = EREF();
        ASTNode* down_arm = new_node(AST_LOGICAL_AND);
        down_arm->binary.left = step_neg; down_arm->binary.right = i_gt_end;

        ASTNode* step_nonneg = new_node(AST_GTE);
        step_nonneg->binary.left = SREF(); step_nonneg->binary.right = zero_lit2;
        ASTNode* i_lt_end = new_node(AST_LT);
        i_lt_end->binary.left = IREF(); i_lt_end->binary.right = EREF();
        ASTNode* up_arm = new_node(AST_LOGICAL_AND);
        up_arm->binary.left = step_nonneg; up_arm->binary.right = i_lt_end;

        cond = new_node(AST_LOGICAL_OR);
        cond->binary.left = down_arm; cond->binary.right = up_arm;
    }

    // incr: i = i + step
    ASTNode* add = new_node(AST_ADD); add->binary.left = IREF(); add->binary.right = SREF();
    ASTNode* incr = new_node(AST_ASSIGN); incr->binary.left = IREF(); incr->binary.right = add;
    #undef IREF
    #undef EREF
    #undef SREF

    s_loop_depth++;
    ASTNode* body = parse_block_body();
    s_loop_depth--;

    ASTNode* node = new_node(AST_FOR);
    node->for_stmt.init = init;
    node->for_stmt.cond = cond;
    node->for_stmt.incr = incr;
    node->for_stmt.body = body;

    s_symtable = prev_table;
    return node;
}

static ASTNode* parse_break_statement(void) {
    if (s_loop_depth == 0) parse_error("'break' outside of a loop");
    advance();
    if (s_curr.type == TOK_SEMI) advance();
    return new_node(AST_BREAK);
}

static ASTNode* parse_continue_statement(void) {
    if (s_loop_depth == 0) parse_error("'continue' outside of a loop");
    advance();
    if (s_curr.type == TOK_SEMI) advance();
    return new_node(AST_CONTINUE);
}

static ASTNode* parse_return_statement(void) {
    advance();
    ASTNode* node = new_node(AST_RETURN);
    node->unary = NULL;
    // Bare return if followed by ; } or EOF; otherwise parse the value.
    if (s_curr.type != TOK_SEMI && s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        node->unary = parse_expr_prec(0);
    }
    if (s_curr.type == TOK_SEMI) advance();
    return node;
}

static ASTNode* parse_defer_statement(void) {
    advance();
    ASTNode* node = new_node(AST_DEFER);
    node->unary = parse_statement();
    return node;
}

static ASTNode* parse_delete_statement(void) {
    advance();
    ASTNode* node = new_node(AST_DELETE);
    node->delete_expr.ptr = parse_expr_prec(0);
    if (s_curr.type == TOK_SEMI) advance();
    return node;
}

static ASTNode* parse_decl_or_expr_statement(void) {
    ParseCheckpoint before_type;
    parser_save(&before_type);
    Type* type = parse_type();
    // A real declaration's name is followed by '=', a statement terminator, or
    // another statement -- never '(' (there is no "declare and immediately call
    // it" grammar). A parenthesized type immediately followed by NAME( is
    // therefore always a CAST applied to a call result (`(T)f(x)`), not a
    // declaration, even though `parse_type()` happily accepts `(T)` as a type
    // spelling on its own (parens are pure grouping in the type grammar). Back
    // out and re-parse the whole thing as an expression statement instead of
    // misreading `f` as a fresh local -- which used to silently shadow an
    // existing function of that name, dropping the call entirely with no error.
    if (type != NULL && s_curr.type == TOK_IDENTIFIER) {
        LexerState after_name; Lexer_Save(&after_name);
        Token name_tok = s_curr;
        advance();
        bool looks_like_call = (s_curr.type == TOK_LPAREN);
        Lexer_Restore(&after_name);
        s_curr = name_tok;
        if (looks_like_call) {
            parser_restore(&before_type);
            type = NULL;
        }
    }
    if (type != NULL) {
        if (s_curr.type != TOK_IDENTIFIER) {
            parse_error("Expected identifier after type");
        }
        Token id_tok = s_curr;
        advance();

        ASTNode* init_expr = NULL;
        if (s_curr.type == TOK_EQ) {
            advance();
            // Init is any expression. A bare `{...}` aggregate literal parses in
            // primary position (UNTYPED) and is resolved against `type` in typecheck
            // (resolve_brace_literal). No array-specific special-case needed here.
            init_expr = parse_expr_prec(0);
        }

        if (s_curr.type == TOK_SEMI) advance();

        SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
        Symbol* sym = SymTable_Add(s_symtable, id_tok.start, id_tok.length, type, kind);

        // Global (static) initializers are constant expressions — the 4th ConstEval
        // use site, alongside array sizes, for-step, and struct field defaults. This
        // keeps one compile-time evaluation model everywhere an initial value is
        // needed, and means a static gets a folded value baked into its image with
        // NO runtime init code and NO pre-main initialization phase (which would
        // otherwise reintroduce C++-style static init-order hazards). Runtime-
        // computed startup state belongs in main, explicitly.
        if (kind == SYM_GLOBAL && init_expr) {
            if (Type_IsAggregate(type)) {
                // Aggregate global: fold the literal into bytes (constexpr now
                // builds aggregates — the old "integer-only" restriction is lifted).
                resolve_brace_literal(init_expr, type);   // parse-time literal typing
                uint64_t sz = Type_SizeOf(type);
                uint8_t* bytes = (uint8_t*)calloc(1, sz ? sz : 1);
                if (!ConstEval_Bytes(init_expr, bytes, sz)) {
                    parse_error("aggregate global initializer is not a constant expression");
                }
                sym->global_bytes = bytes;
                sym->has_init = true;
                init_expr = NULL;
            } else {
                int64_t gval;
                if (!ConstEval(init_expr, &gval)) {
                    parse_error("global initializer is not a constant expression");
                }
                sym->global_init = gval;
                sym->has_init = true;
                init_expr = NULL; // value lives in the static image, not in runtime code
            }
        }

        // init_expr is NULL here for a global with a real value (the const-fold
        // branch above bakes it into the static image instead -- a global never
        // gets runtime init code, so it must never become a synthesized assignment).
        return make_decl_stmt(type, id_tok.start, id_tok.length, sym, init_expr);
    }

    ASTNode* expr = parse_expr_prec(0);
    if (s_curr.type == TOK_SEMI) advance();
    return expr;
}

static ASTNode* parse_statement(void) {
    // An alias is legal as a STATEMENT, not only at file scope. It binds a name to a
    // Type* -- no value, no storage, no codegen -- so a block-local one is exactly as
    // safe as a block-local variable, and lets an interface / vtable type be declared
    // where it is used instead of polluting the file namespace.
    if (s_curr.type == TOK_ALIAS) {
        return parse_alias_decl();
    }
    if (s_curr.type == TOK_UNPACK) {
        return parse_unpack();
    }
    if (s_curr.type == TOK_WITH) {
        return parse_top_level(); // with desugars to top-level decls; valid in local scope too
    }
    if (s_curr.type == TOK_MATCH) {
        return parse_match();
    }
    // `if` is a STATEMENT, not an expression -- same demotion `match` already has
    // above. AST_IF had no real result-handling in codegen: both arms happened to
    // leave their last value in rax (every expression does), and a caller reading
    // rax as "the if's value" was a register-allocator coincidence for scalars,
    // not a real feature (see docs/KNOWN_BUGS.md). Using `if` in a value position
    // now fails the same way `match` already does in expression position: a plain
    // parse error, since it's no longer reachable from parse_primary at all.
    if (s_curr.type == TOK_IF) {
        return parse_if_expr();
    }
    if (s_curr.type == TOK_WHILE) {
        return parse_while_statement();
    }
    if (s_curr.type == TOK_FOR) {
        return parse_for_statement();
    }
    if (s_curr.type == TOK_BREAK) {
        return parse_break_statement();
    }
    if (s_curr.type == TOK_CONTINUE) {
        return parse_continue_statement();
    }
    if (s_curr.type == TOK_RETURN) {
        return parse_return_statement();
    }
    if (s_curr.type == TOK_DEFER) {
        return parse_defer_statement();
    }
    if (s_curr.type == TOK_DELETE) {
        return parse_delete_statement();
    }
    if (s_curr.type == TOK_LBRACE) {
        return parse_block_body();
    }
    if (s_curr.type == TOK_CONST) {
        // `const { stmts }` -- a STATEMENT, exactly like `defer { stmts }`, never
        // an expression (there is no `TYPE x = const { ... }`, the same way there
        // is no `TYPE x = if cond { .. } else { .. }` in this language). Unlike
        // `const TYPE name = expr` below (which creates a NEW symbol that VANISHES
        // -- inlined as a literal, no runtime storage), this form assigns a
        // comptime-folded value INTO an already-declared, genuinely-runtime
        // variable: `u32 x  const { x = expr }` -- x is a real local/global,
        // only its fill is computed at compile time. This is the ONE mechanism
        // for a forced compile-time fold now -- there used to also be an inline
        // `const(expr)` operator reachable from expression position (any
        // sub-expression, anywhere), removed in favor of this single statement
        // form: declare a temp first, fold into it here, use the temp. Same
        // discipline `if`/`match` already have as statements, never expressions.
        {
            LexerState csave; Lexer_Save(&csave); Token ccur_save = s_curr;
            advance(); // 'const'
            bool is_block = (s_curr.type == TOK_LBRACE);
            Lexer_Restore(&csave); s_curr = ccur_save;
            if (is_block) return parse_const_block();
        }
        return parse_const_decl(false);
    }

    return parse_decl_or_expr_statement();
}

// Parses both `struct Name {...}` and `enum Name {...}` -- they share the registry,
// field machinery, sizeof and cycle detection. An enum's "fields" are VARIANTS:
// `type name` (payload) or bare `name` (no payload). Layout differs (tag+max vs sum),
// handled in Struct_Layout via sd->is_enum.
static ASTNode* parse_struct_decl_ex(bool is_enum, bool is_overlapping, bool is_pub) {
    advance(); // consume 'struct' / 'enum' / 'union'
    if (s_curr.type != TOK_IDENTIFIER) parse_error(is_enum ? "Expected enum name" : (is_overlapping ? "Expected union name" : "Expected struct name"));
    Token name = s_curr;
    advance();

    // Register up front so fields may reference the type by pointer (self-ref ok).
    StructDef* sd = Struct_Register(name.start, name.length);
    if (sd->field_count > 0 || sd->laid_out) parse_error("type redefinition");
    sd->is_enum = is_enum;
    sd->is_overlapping = is_overlapping;
    sd->is_pub = is_pub;

    // Generic type-parameter list: struct Vec[T, U] { ... }. Stencil model — the
    // generic is a template, never laid out; each Name[args] use is instantiated.
    const char** prev_tparams = s_type_params;
    size_t prev_tparam_count = s_type_param_count;
    Type** prev_pkinds = s_param_kinds;
    if (s_curr.type == TOK_LBRACKET) {
        const char** tparams = NULL; Type** pkinds = NULL; size_t pcount = 0;
        parse_generic_param_list(&tparams, &pkinds, &pcount);
        sd->is_generic = true;
        sd->type_params = tparams;
        sd->param_kinds = pkinds;
        sd->type_param_count = pcount;
        s_type_params = tparams;        // resolve T within field types
        s_param_kinds  = pkinds;
        s_type_param_count = pcount;
    }

    if (s_curr.type != TOK_LBRACE) parse_error(is_overlapping ? "Expected '{' after union name" : "Expected '{' after struct name");
    advance();

    size_t cap = 8;
    sd->fields = (StructField*)calloc(cap, sizeof(StructField));
    sd->field_count = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        // `super TypeName fieldname` -- embeds another struct's fields at this
        // point in the field list (so `d.x` reaches a field promoted from the
        // embedded type), plus `fieldname` itself as an ordinary field of the
        // embedded type (so `d.fieldname` gives the whole embedded value back).
        // Contextual identifier, same convention as `by` in `for` -- not a
        // reserved keyword. Simplest possible form: no check that TypeName is
        // itself a struct in any particular shape, no check against enum/union
        // (it splices the same way there too, whether or not that's meaningful),
        // no cycle check beyond whatever Struct_Layout already does for any
        // struct-typed field. Struct_Layout lays out whatever fields[] ends up
        // holding, in declaration order, exactly as if the embedded fields had
        // been typed out by hand -- so nothing downstream (layout, codegen,
        // typecheck) needs to know `super` exists at all.
        if (s_curr.type == TOK_IDENTIFIER && s_curr.length == 5 &&
            strncmp(s_curr.start, "super", 5) == 0) {
            advance(); // 'super'
            // Go through the real type parser rather than a bare identifier lookup --
            // it already handles a generic instantiation (`super Box[i32] base`), an
            // alias, or anything else a field type can be.
            Type* super_pt = parse_type();
            if (!super_pt) parse_error("Expected type name after 'super'");
            if (super_pt->cls != TYPE_STRUCT && super_pt->cls != TYPE_PARAM)
                parse_error("'super' type must be a struct/enum/union, or this template's own type parameter");
            if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected field name after 'super TypeName'");
            Token super_field_name = s_curr;
            advance();

            char* base_name = strndup(super_field_name.start, super_field_name.length);

            if (super_pt->cls == TYPE_PARAM) {
                // `super T base` where T is still an unresolved type parameter of the
                // enclosing generic template -- there is no StructDef to promote fields
                // FROM yet (T isn't bound to anything until instantiation). Record just
                // the `base`-shaped field (type T), flagged is_super_param, and defer the
                // actual promotion-splice to Struct_Instantiate, once T is concrete.
                StructField f = { .name = base_name, .type = super_pt };
                Struct_AppendField(&sd->fields, &sd->field_count, &cap, f);
                sd->fields[sd->field_count - 1].is_super_param = true;
                free(base_name);

                if (s_curr.type == TOK_SEMI) advance(); // optional separator
                continue;
            }

            StructDef* super_sd = Struct_Find(super_pt->struct_name);
            if (!super_sd) parse_error("Unknown type after 'super'");

            for (size_t si = 0; si < super_sd->field_count; si++) {
                Struct_AppendField(&sd->fields, &sd->field_count, &cap, super_sd->fields[si]);
            }

            // The packaged field `d.base` ALIASES the promoted prefix (single
            // storage) rather than owning a second copy: flag it and record how
            // many promoted fields precede it, so Struct_Layout points its offset
            // at the prefix start and gives it zero size.
            // TYPE_STRUCT, already correctly resolved by parse_type (handles generics/aliases)
            StructField f = { .name = base_name, .type = super_pt,
                              .is_super_alias = true,
                              .super_prefix_span = (uint32_t)super_sd->field_count };
            Struct_AppendField(&sd->fields, &sd->field_count, &cap, f);
            free(base_name);

            if (s_curr.type == TOK_SEMI) advance(); // optional separator
            continue;
        }

        Type* ftype = NULL;
        bool has_default = false;
        uint8_t* default_val_buf = NULL;

        if (is_enum) {
            // Variant: `type name` (payload) or bare `name` (no payload). The payload
            // type is optional, so peek -- if a type starts here, the variant carries
            // it; otherwise the variant is just a tag.
            if (curr_begins_type()) {
                ftype = parse_type();
                if (!ftype) parse_error("Expected variant payload type");
            }
            if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected variant name in enum");
        } else {
            ftype = parse_type();
            if (!ftype) parse_error(is_overlapping ? "Expected field type in union" : "Expected field type in struct");
            if (s_curr.type != TOK_IDENTIFIER) parse_error(is_overlapping ? "Expected field name in union" : "Expected field name in struct");
        }
        Token fname = s_curr;
        advance();

        if (!is_enum && s_curr.type == TOK_EQ) {
            advance();
            ASTNode* dexpr = parse_expr_prec(0);
            
            resolve_brace_literal(dexpr, ftype);
            uint64_t sz = Type_SizeOf(ftype);
            uint8_t* bytes = (uint8_t*)calloc(1, sz ? sz : 1);
            if (!ConstEval_Bytes(dexpr, bytes, sz)) {
                parse_error("field default must be a constant expression");
            }
            default_val_buf = bytes;
            has_default = true;
        }

        if (sd->field_count >= cap) {
            cap *= 2;
            sd->fields = (StructField*)realloc(sd->fields, cap * sizeof(StructField));
        }
        size_t this_idx = sd->field_count;
        StructField* f = &sd->fields[sd->field_count++];
        f->name = strndup(fname.start, fname.length);
        f->type = ftype;          // NULL for a no-payload variant
        f->offset = 0;
        f->has_default = has_default;
        f->default_val_buf = default_val_buf;
        f->is_super_param = false;
        f->variant_tag = is_enum ? (uint32_t)this_idx : 0; // meaningless for struct/union

        if (s_curr.type == TOK_SEMI) advance(); // optional separator
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end struct");
    advance();
    // Optional trailing ';' after the closing brace — same convention as
    // const declarations (parse_const_decl, below) — so a C-style
    // `struct Foo { ... };` doesn't leave a stray ';' for the next
    // top-level parse to choke on.
    if (s_curr.type == TOK_SEMI) advance();

    s_type_params = prev_tparams;
    s_param_kinds  = prev_pkinds;
    s_type_param_count = prev_tparam_count;

    // Generic templates are never laid out; concrete structs/enums are laid out now.
    if (sd->is_generic) {
        Struct_UpdateInstantiations(sd);
    }

    ASTNode* node = new_node(AST_STRUCT_DECL);
    return node; // emits no code
}



// `const { stmts }` -- a STATEMENT (like `defer`, never an expression: there is
// no `TYPE x = const { .. }`). Parses an ordinary block via parse_block_body
// (so scoping, nested blocks, everything about it is a plain `{ }` -- `const`
// itself contributes no scoping, exactly like `defer` contributes none; the
// braces do that on their own). Then walks the block's TOP-LEVEL statements:
// any bare assignment `x = expr` (x already declared, ordinary runtime
// variable -- this does NOT create a new symbol, unlike `const TYPE name =
// expr`) has its RHS constant-folded in place, so `x` ends up a real runtime
// variable whose fill was computed at compile time. Handles the same
// generic-body deferred-fold case the old inline `const(expr)` operator did:
// an assignment whose RHS mentions an in-scope generic param can't fold NOW
// (the template body is parsed once, before any instantiation exists), so
// it's wrapped in AST_CONST_EXPR -- the SAME node clone_ast already re-folds
// per instantiation (see its AST_CONST_EXPR case) -- instead of failing.
static ASTNode* parse_const_block(void) {
    advance(); // consume 'const'
    ASTNode* block = parse_block_body();

    for (size_t i = 0; i < block->block.count; i++) {
        ASTNode* stmt = block->block.statements[i];
        if (stmt->type != AST_ASSIGN) continue; // only bare `x = expr` folds; anything
                                                   // else (if/while/nested block/decl)
                                                   // is left as an ordinary statement.
        ASTNode* rhs = stmt->binary.right;

        if (s_type_param_count > 0 && expr_mentions_generic_param(rhs)) {
            ASTNode* deferred = new_node(AST_CONST_EXPR);
            deferred->const_expr.inner = rhs;
            stmt->binary.right = deferred;
            continue;
        }

        int64_t value = 0;
        s_ce_isfloat = false;
        if (!ConstEval(rhs, &value))
            parse_error("const { } assignment operand is not a constant expression");

        ASTNode* lit = new_node(AST_INT_LITERAL);
        if (s_ce_isfloat) {
            lit->lit_kind = LIT_FLOAT;
            double d; memcpy(&d, &value, sizeof d);
            lit->float_value = d;
        } else {
            lit->lit_kind = LIT_INT;
            lit->int_value = (uint64_t)value;
        }
        stmt->binary.right = lit;
    }

    return block;
}

static ASTNode* parse_const_decl(bool is_pub) {
    advance(); // consume 'const'

    // Type-first: `const TYPE name = constexpr`, consistent with every other binding
    // (var decls, params, fields, `for`). Replaces the old untyped `const NAME = ..`
    // whose type was inferred i32/i64 -- the last inferred binding in the language.
    Type* t = parse_type();
    if (!t) parse_error("Expected type after 'const' (e.g. `const u32 MAX = 1024`)");
    bool is_void = t->cls == TYPE_PRIMITIVE && (t->primitive == PRIM_VOID || t->primitive == PRIM_V);
    if (is_void) parse_error("const type cannot be void");

    if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected name after const type");
    Token name = s_curr;
    advance();
    if (s_curr.type != TOK_EQ) parse_error("Expected '=' in const declaration");
    advance();

    // Aggregate const (array or struct): emit as a read-only global so the
    // name resolves to an addressable value just like any global — codegen
    // already handles aggregate globals via global_bytes.
    bool is_aggregate = Type_IsAggregate(t);
    if (is_aggregate) {
        ASTNode* expr = parse_expr_prec(0);
        resolve_brace_literal(expr, t);
        uint64_t sz = Type_SizeOf(t);
        uint8_t* bytes = (uint8_t*)calloc(1, sz ? sz : 1);
        // Evaluate the initializer ONCE into persistent comptime storage. Works for
        // a literal OR a fn call returning an aggregate (the whole computation runs
        // at compile time). Then copy those exact bytes into global_bytes for the
        // runtime image, and register the const so later consts can address into it.
        int64_t agg_off = ConstEval_AggPersist(expr, t);
        if (agg_off < 0)
            parse_error("const aggregate initializer is not a constant expression");
        if (!ConstEval_ReadBytes((uint32_t)agg_off, bytes, sz))
            parse_error("const aggregate initializer is not a constant expression");
        if (ConstEval_AggHasEscapingPtr(t, bytes, sz))
            parse_error("const aggregate stores a pointer into compile-time memory "
                        "that has no runtime address (a comptime-heap pointer can't "
                        "escape into a const); store a value or an index, not a pointer");
        Symbol* sym = SymTable_Add(s_symtable, name.start, name.length, t, SYM_GLOBAL);
        sym->global_bytes = bytes;
        sym->has_init     = true;
        sym->is_pub       = is_pub;
        // Emission is keyed on carrying folded bytes, not on scope-table membership:
        // this const may have been declared inside a function body, in which case its
        // Symbol lives in that function's table and the global-table walk would miss
        // it (leaving the slot zeroed -- silent wrong values). Register explicitly.
        Global_RegisterForEmit(sym);
        Const_Register(name.start, name.length, agg_off, t);
        if (s_curr.type == TOK_SEMI) advance();
        ASTNode* node = new_node(AST_BLOCK);
        node->block.statements = NULL; node->block.count = 0; node->block.capacity = 0;
        return node;
    }

    // Scalar const: inline as a literal at every use-site (original path).
    if (!(t->cls == TYPE_PRIMITIVE))
        parse_error("const type must be a primitive, array, or struct type");

    size_t pending_before = Const_PendingUseCount();
    ASTNode* expr = parse_expr_prec(0);

    // A `const` inside a generic fn/impl body whose initializer mentions an
    // in-scope generic param (e.g. `const u32 c = N`, or a call to a method of
    // the same generic `impl`) can't be folded here: at this point we're
    // parsing the shared TEMPLATE body, once, before any instantiation exists
    // to say what N even is. Folding it into the single flat `s_consts` table
    // (keyed by name only) would also be wrong even if timing weren't an issue,
    // since two instantiations (e.g. Fixed[i32,5] and Fixed[i32,8]) need two
    // different values under the same name. So: don't fold, don't register
    // globally — emit an ordinary local AST_DECLARATION (exactly what a
    // non-const local produces, which already clones correctly per-instantiation
    // in backend_x64.c's clone_ast) and tag it `is_generic_const` so clone_ast
    // re-folds its initializer with that instantiation's own generic-param
    // frame, once concrete args are known. See clone_ast's AST_DECLARATION case.
    if (s_symtable->is_function_scope && s_type_param_count > 0 && expr_mentions_generic_param(expr)) {
        if (s_curr.type == TOK_SEMI) advance();
        Symbol* sym = SymTable_Add(s_symtable, name.start, name.length, t, SYM_LOCAL);
        ASTNode* node = new_node(AST_DECLARATION);
        node->decl.var_type = t;
        node->decl.name = name.start;
        node->decl.name_len = name.length;
        node->decl.init_expr = expr;
        node->decl.sym = sym;
        node->decl.is_generic_const = true;
        return node;
    }

    // Evaluate the initializer. Try eagerly (works for the common case and is
    // required for consts used as array dimensions in later types). If it can't
    // fold NOW — typically because it forward-references a function defined later
    // in the file — register it as PENDING and Const_ResolvePending() retries it
    // after the whole file is parsed. (Was: immediate "not a constant expression",
    // which forbade const-before-fn.)
    // If the initializer inlined a still-pending const, that dependency forward-
    // references a later fn and was destructively inlined as placeholder 0, so this
    // const can't be correctly deferred (its AST baked the 0). Reject loudly rather
    // than silently fold wrong. (Workaround: declare the function before the consts.)
    int64_t value = 0;
    bool folded = ConstEval(expr, &value);
    if (Const_PendingUseCount() > pending_before)
        parse_error("a const cannot use another const that forward-references a later "
                    "function; declare that function before these consts");

    if (folded) {
        // Literal-fit: the folded value must fit the declared integer type.
        if (t->primitive != PRIM_F32 && t->primitive != PRIM_F64) {
            int w = Type_Width(t);
            bool sgn = Type_IsSigned(t);
            bool fits = true;
            if (w < 8) {
                if (sgn) {
                    int64_t lo = -(1LL << (w * 8 - 1));
                    int64_t hi =  (1LL << (w * 8 - 1)) - 1;
                    fits = (value >= lo && value <= hi);
                } else {
                    uint64_t hi = (w == 8) ? ~0ULL : ((1ULL << (w * 8)) - 1);
                    fits = ((uint64_t)value <= hi) || (value >= 0 && (uint64_t)value <= hi);
                    if (value < 0) fits = false;
                }
            }
            if (!fits) parse_error("const value does not fit its declared type (use a cast to truncate)");
        }
    }

    ConstDef* cdef = Const_Register(name.start, name.length, value, t);
    cdef->is_pub = is_pub;
    if (!folded) cdef->pending_expr = expr;

    Symbol* sym = SymTable_Add(s_symtable, name.start, name.length, t, SYM_CONST);
    sym->is_pub = is_pub;
    sym->cdef = cdef;

    if (s_curr.type == TOK_SEMI) advance();
    // A const emits no code; return a harmless empty block.
    ASTNode* node = new_node(AST_BLOCK);
    node->block.statements = NULL;
    node->block.count = 0;
    node->block.capacity = 0;
    return node;
}

static ASTNode* parse_struct_decl(bool is_pub) { return parse_struct_decl_ex(false, false, is_pub); }
static ASTNode* parse_enum_decl(bool is_pub)   { return parse_struct_decl_ex(true, false, is_pub); }
static ASTNode* parse_union_decl(bool is_pub)  { return parse_struct_decl_ex(false, true, is_pub); }

// `alias Name = <type>` / `alias Name[P,...] = <type>`.
//
// SCOPED. Callable from parse_top_level (file scope) AND parse_statement (block scope).
// An alias binds a NAME to a Type* -- it allocates nothing, emits nothing, and outlives
// nothing, so there was never a reason for it to be the one binder in the language forced
// to be global. Block scoping needs no new machinery: parse_block_body already pushes a
// SymbolTable scope, alias_lookup already scans backward (so an inner binding shadows an
// outer one by construction), and a scope exit is just truncating s_alias_count back to
// the mark taken on entry.
static ASTNode* parse_alias_decl(void) {
    advance(); // 'alias'
    if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected name after 'alias'");
    char*  aname = strndup(s_curr.start, s_curr.length);
    size_t alen  = s_curr.length;
    advance();

    // Optional [P, ...] type params -> installed into scope so the body's
    // holes resolve to TYPE_PARAM. Prototype supports type params only.
    const char** prev_tp = s_type_params; Type** prev_pk = s_param_kinds; size_t prev_c = s_type_param_count;
    const char** aparams = NULL; Type** akinds = NULL; size_t apc = 0;
    if (s_curr.type == TOK_LBRACKET) {
        const char** names = NULL; Type** kinds = NULL; size_t cnt = 0;
        parse_generic_param_list(&names, &kinds, &cnt);
        aparams = names; akinds = kinds; apc = cnt;
        s_type_params = names; s_param_kinds = kinds; s_type_param_count = cnt;
    }

    if (s_curr.type != TOK_EQ) parse_error("Expected '=' in alias declaration");
    advance();

    // An alias NAMES a Type*, it does not DECLARE a value -- so its right-hand side
    // is a pattern position, not a value position. That distinction is the whole
    // reason the impl guard lives at the producer and keys on "am I parsing a
    // pattern" rather than on "is this an impl": `alias Freeable = impl { fn free() }`
    // is exactly the useful case (a NAMED structural interface, reusable by name in
    // any `match` arm), and it falls out for free once the flag is set here. No
    // special case for alias, no special case for impl -- alias simply declares what
    // kind of position it is, the same way a match arm does.
    // An alias NAMES a Type*, so its RHS is a pattern position -- `impl {...}` may appear
    // (`alias Freeable = impl { fn free() }`, a named structural interface).
    //
    // But it sets ONLY s_pattern_types_ok, NOT s_in_match_pattern. Those are two different
    // questions, and conflating them was a real bug: s_in_match_pattern also means "an
    // undeclared identifier here is a WILDCARD", which in an alias body turns every typo
    // into a silent 8-byte mystery type (`alias Buf = u8x` compiled clean). An alias body
    // has no wildcards -- its own params (`alias P[X] = X*`) are already in s_type_params
    // and resolve through the ordinary path -- so an unknown name there is exactly what it
    // looks like: an error.
    bool prev_pt = s_pattern_types_ok;
    s_pattern_types_ok = true;
    Type* body = parse_type();
    s_pattern_types_ok = prev_pt;
    if (!body) parse_error("Expected a type on the right-hand side of alias");

    // restore scope
    s_type_params = prev_tp; s_param_kinds = prev_pk; s_type_param_count = prev_c;

    if (s_alias_count >= s_alias_cap) {
        s_alias_cap = s_alias_cap ? s_alias_cap * 2 : 8;
        s_aliases = realloc(s_aliases, s_alias_cap * sizeof(AliasDef));
    }
    s_aliases[s_alias_count++] = (AliasDef){ aname, alen, aparams, akinds, apc, body };

    if (s_curr.type == TOK_SEMI) advance(); // optional separator, same as every sibling declaration

    // No codegen artifact: an alias is a parse-time-only binding.
    ASTNode* empty = new_node(AST_BLOCK);
    empty->block.capacity = 0; empty->block.count = 0; empty->block.statements = NULL;
    return empty;
}

// `with PREFIX... { body }` — declaration grouping. Desugars each newline-
// delimited entry in body by prepending the prefix tokens and re-parsing as
// a normal top-level declaration. Pure token-replay: no new semantics, the
// parser functions downstream never know `with` existed.
static ASTNode* parse_with_block(void) {
    advance(); // consume 'with'

    // Collect prefix tokens (everything before '{').
    // We store each token's (LexerState-before, token) so we can replay.
    typedef struct { LexerState st; Token tok; bool nl; } PrefixTok;
    size_t pcap = 8, pcount = 0;
    PrefixTok* ptoks = (PrefixTok*)malloc(pcap * sizeof(PrefixTok));
    while (s_curr.type != TOK_LBRACE && s_curr.type != TOK_EOF) {
        if (pcount >= pcap) { pcap *= 2; ptoks = realloc(ptoks, pcap * sizeof(PrefixTok)); }
        Lexer_Save(&ptoks[pcount].st);
        ptoks[pcount].tok = s_curr;
        ptoks[pcount].nl  = s_curr_newline_before;
        pcount++;
        advance();
    }
    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' after 'with' prefix");
    if (pcount == 0) parse_error("'with' requires at least one prefix token before '{'");
    advance(); // consume '{'

    ASTNode* block = new_node(AST_BLOCK);
    block->block.capacity = 16;
    block->block.statements = (ASTNode**)malloc(block->block.capacity * sizeof(ASTNode*));
    block->block.count = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        if (s_curr.type == TOK_SEMI) { advance(); continue; }

        // s_curr = entry's first token; lexer points at entry's second token.
        Token entry_tok = s_curr;
        bool  entry_nl  = s_curr_newline_before;
        LexerState after_entry_tok;
        Lexer_Save(&after_entry_tok);

        // Set up prefix replay: s_curr = ptoks[0], lexer at ptoks[0].st so
        // advance() naturally re-lexes ptoks[1..N-1] from the prefix source.
        // After pcount-1 advances, the switch fires: lexer jumps to
        // after_entry_tok and s_curr becomes entry_tok.
        s_curr                = ptoks[0].tok;
        s_curr_newline_before = ptoks[0].nl;
        Lexer_Restore(&ptoks[0].st);

        s_with_switch_after  = (int)pcount - 1;
        s_with_switch_st     = after_entry_tok;
        s_with_switch_tok    = entry_tok;
        s_with_switch_nl     = entry_nl;
        s_with_switch_active = true;

        ASTNode* decl = parse_top_level();
        s_with_switch_active = false;
        if (decl) append_block_statement(block, decl);
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to close 'with' block");
    advance(); // consume '}'
    free(ptoks);
    return block;
}

static ASTNode* parse_top_level(void) {
    if (s_curr.type == TOK_WITH) return parse_with_block();

    bool is_pub = false;
    if (s_curr.type == TOK_PUB) {
        is_pub = true;
        advance();
    }

    if (s_curr.type == TOK_CONST) {
        return parse_const_decl(is_pub);
    }
    if (s_curr.type == TOK_STRUCT) {
        return parse_struct_decl(is_pub);
    }
    if (s_curr.type == TOK_ENUM) {
        return parse_enum_decl(is_pub);
    }
    if (s_curr.type == TOK_UNION) {
        return parse_union_decl(is_pub);
    }
    // `alias Name = <type>`  or  `alias Name[P, ...] = <type>`
    if (s_curr.type == TOK_ALIAS) {
        return parse_alias_decl();
    }
    // impl TypeName { fn method(...) RT { ... } ... }
    if (s_curr.type == TOK_IMPL) return parse_impl_block(is_pub);

    bool is_extern = false;
    if (s_curr.type == TOK_EXTERN) {
        is_extern = true;
        advance();
        if (s_curr.type != TOK_FN) parse_error("Expected 'fn' after 'extern'");
    }

    { ASTNode* r = parse_fn_decl(is_pub, is_extern, NULL, 0, NULL); if (r) return r; }
}

// impl TypeName { fn method(...) RT { ... } ... }
// Desugars each method: prepends `Foo* self` as first param, mangles name to Foo_method.
// Produces a block of normal AST_FUNC_DECL nodes — no new AST node type needed.
static ASTNode* parse_impl_block(bool is_pub) {
    advance(); // consume 'impl'
    if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected type name after 'impl'");
    const char* impl_type = strndup(s_curr.start, s_curr.length);
    size_t impl_type_len  = s_curr.length;
    advance(); // consume type name

    // `impl` is a type site, and an alias is interchangeable with what it names
    // (§24). It cannot simply call parse_type(): the bracket after the name here
    // *declares* the generic params (`impl Vector[T]`) rather than instantiating
    // them, so the common path would try to resolve `T` and fail. It resolves the
    // alias directly instead -- which is all the common path would have done for a
    // bare name anyway.
    //
    //   alias S = Raw          ->  `impl S` attaches to Raw.
    //   alias S = Vector[u8]   ->  `impl S` attaches to the INSTANTIATED
    //                              StructDef, whose registry name is literally
    //                              "Vector[u8]" -- exactly the name a call site
    //                              mangles with, so `s.method()` resolves.
    //
    // Without this, impl_type stayed the raw token text ("S"), Struct_Find missed,
    // and `self` was built as a struct nobody had ever declared -- surfacing much
    // later as "field access on a non-struct value". This is the failure mode
    // docs/type_grammar.md warns about: a call site that stopped trusting the
    // common path.
    {
        AliasDef* impl_al = alias_lookup(impl_type, impl_type_len);
        if (impl_al && impl_al->param_count == 0 &&
            impl_al->body && impl_al->body->cls == TYPE_STRUCT) {
            StructDef* target = Struct_Find(impl_al->body->struct_name);
            // An alias to a generic INSTANTIATION (`alias String = Vector[u8]`) is
            // the same specialization request as `impl Vector[u8]`, arriving under a
            // different name -- and the call site cannot reach it either, because it
            // mangles with the generic base (`Vector_f`, not `Vector[u8]_f`). Left
            // alone it would register a symbol nothing can ever call: the method
            // would simply not exist, with no diagnostic. Say so instead.
            if (target && target->generic_base) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "cannot impl on `%.*s`: it aliases the generic instantiation "
                         "`%s`, and a method on one instantiation (specialization) is "
                         "not supported -- impl the generic base (`impl %s[T]`) instead",
                         (int)impl_type_len, impl_type, target->name,
                         target->generic_base->name);
                parse_error(buf);
            }
            free((void*)impl_type);                       // drop the strndup'd token text
            impl_type     = impl_al->body->struct_name;   // stable: owned by the struct registry
            impl_type_len = strlen(impl_type);
        }
    }

    // Optional [T, U] — consume (just for syntax), then read type_params from the
    // StructDef itself (single source of truth). All methods in this block share them.
    //
    // The bracket here DECLARES the generic parameters; it does not instantiate
    // them. `impl Vector[T]` and `impl Vector[u8]` would otherwise both register one
    // symbol on the generic base (`Vector_f`), because the call site mangles with the
    // base name (types.c, try_rewrite_method_call) and monomorphizes per
    // instantiation. So `impl Vector[u8]` used to READ as "a method only on
    // Vector[u8]", compile clean, and then answer calls on Vector[u32] as well --
    // silent wrongness, which §1 rejects outright. Specialization on a concrete
    // instantiation is a real feature and a real design decision (it is where
    // coherence rules come from); an ignored bracket should not grant it by accident.
    //
    // A slot is a PARAMETER DECLARATION iff it ENDS in a name that is not already a
    // type -- which covers both forms uniformly and needs no lookahead:
    //
    //     T              ends in `T`  -- unknown identifier   -> type param    OK
    //     u32 N          ends in `N`  -- unknown identifier   -> value param   OK
    //     u32[4] W       ends in `W`  -- unknown identifier   -> value param   OK
    //     u8             ends in `u8` -- a primitive keyword  -> SPECIALIZATION
    //     Foo            ends in `Foo`-- a registered type    -> SPECIALIZATION
    //
    // (The last token of a slot is the one sitting before the `,` or the closing `]`,
    // so this is decided by remembering the previous token rather than by parsing.)
    if (s_curr.type == TOK_LBRACKET) {
        int depth = 1; advance();
        Token prev = s_curr;
        while (depth > 0 && s_curr.type != TOK_EOF) {
            if (s_curr.type == TOK_LBRACKET) depth++;
            else if (s_curr.type == TOK_RBRACKET) depth--;

            bool slot_ends_here = (depth == 1 && s_curr.type == TOK_COMMA) || depth == 0;
            if (slot_ends_here && prev.type != TOK_LBRACKET) {
                bool is_param_name = (prev.type == TOK_IDENTIFIER) &&
                                     !token_begins_type(prev);
                if (!is_param_name) {
                    char buf[320];
                    snprintf(buf, sizeof(buf),
                             "impl on a concrete generic instantiation is not supported "
                             "(specialization): `%.*s` in `impl %.*s[...]` names a TYPE, not a "
                             "parameter. A method declared in an impl block applies to every "
                             "instantiation, so write `impl %.*s[T]` and match on T inside if "
                             "the behavior must differ",
                             (int)prev.length, prev.start,
                             (int)impl_type_len, impl_type,
                             (int)impl_type_len, impl_type);
                    parse_error(buf);
                }
            }
            if (depth > 0) { prev = s_curr; advance(); } else advance();
        }
    }

    // Install struct's type params into scope for all methods in this block.
    const char** prev_impl_tparams = s_type_params;
    Type**       prev_impl_pkinds  = s_param_kinds;
    size_t prev_impl_tparam_count  = s_type_param_count;
    char* impl_type_nul = strndup(impl_type, impl_type_len);
    StructDef* impl_sd = Struct_Find(impl_type_nul);
    free(impl_type_nul);
    if (impl_sd && impl_sd->type_param_count > 0) {
        s_type_params      = impl_sd->type_params;
        s_param_kinds      = impl_sd->param_kinds;
        s_type_param_count = impl_sd->type_param_count;
    }

    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' after impl type name");
    advance(); // consume '{'

    ASTNode* block = new_node(AST_BLOCK);
    block->block.capacity = 8;
    block->block.statements = (ASTNode**)malloc(block->block.capacity * sizeof(ASTNode*));
    block->block.count = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        if (s_curr.type != TOK_FN) parse_error("Expected 'fn' inside impl block");
        // parse_fn_decl handles the whole method: name mangling, self
        // injection, generic-param merge with the struct's own, body.
        ASTNode* node = parse_fn_decl(is_pub, false, impl_type, impl_type_len, impl_sd);

        // parse_fn_decl restores s_type_params to whatever was active on
        // entry -- which for a method with no [U] extension already is the
        // struct's params (untouched), but a method WITH one leaves the
        // merged copy active. Re-seed from impl_sd so the next method in
        // this block starts clean, matching the pre-merge behavior.
        s_type_params      = impl_sd ? impl_sd->type_params      : prev_impl_tparams;
        s_param_kinds      = impl_sd ? impl_sd->param_kinds      : prev_impl_pkinds;
        s_type_param_count = impl_sd ? impl_sd->type_param_count : prev_impl_tparam_count;

        append_block_statement(block, node);
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to close impl block");
    advance();
    s_type_params      = prev_impl_tparams;
    s_param_kinds      = prev_impl_pkinds;
    s_type_param_count = prev_impl_tparam_count;
    return block;
}

// A function declaration -- `fn`, `pub fn`, `extern fn`, and the `fn`s inside an `impl`
// block. Split out of parse_top_level, which was 652 lines with this inlined while three
// sibling sub-parsers (parse_struct_decl_ex / parse_alias_decl / parse_const_decl)
// already existed. Same treatment, applied to the one that never got it.
//
// Takes the modifiers as parameters because they are consumed BEFORE the dispatch
// (`pub extern fn ...`), so they cannot be re-read from s_curr here.
static ASTNode* parse_fn_decl(bool is_pub, bool is_extern,
                              const char* impl_type_name, size_t impl_type_len,
                              StructDef* impl_sd) {
    if (s_curr.type == TOK_FN) {
        const char* p = s_curr.start + s_curr.length;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '(') {
            // Function pointer variable declaration (type starts with fn() )
            ASTNode* block = new_node(AST_BLOCK);
            block->block.capacity = 16;
            block->block.statements = (ASTNode**)malloc(block->block.capacity * sizeof(ASTNode*));
            block->block.count = 0;
            ASTNode* stmt = parse_statement();
            if (stmt) {
                block->block.statements[block->block.count++] = stmt;
            }
            return block;
        }
        advance();
        if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected function name");
        Token name = s_curr;
        advance();

        // Inside an impl block, the parsed name mangles to `TYPE_method`.
        char* mname = NULL;
        size_t mname_len = 0;
        if (impl_type_name) {
            mname_len = impl_type_len + 1 + name.length;
            mname = (char*)malloc(mname_len + 1);
            memcpy(mname, impl_type_name, impl_type_len);
            mname[impl_type_len] = '_';
            memcpy(mname + impl_type_len + 1, name.start, name.length);
            mname[mname_len] = '\0';
        }

        // Generic parameter list: fn name[T, u32 N](...). Type params are bare
        // names; value params are `type name` (e.g. `u32 N`), pinned like function
        // parameters. Installed into s_type_params/s_param_kinds so parse_type and
        // value-arg folding resolve them across this function's signature and body.
        // Inside an impl block, the struct's own type params (already the active
        // s_type_params from the impl-block scope) seed this list, so a method's
        // optional `[U]` extension merges with them rather than replacing them.
        const char** tparams = NULL;
        Type**       pkinds  = NULL;
        size_t tparam_count = 0;
        const char** prev_tparams = s_type_params;
        Type**       prev_pkinds  = s_param_kinds;
        size_t prev_tparam_count = s_type_param_count;
        if (s_curr.type == TOK_LBRACKET) {
            parse_generic_param_list_with_prefix(prev_tparams, prev_pkinds, prev_tparam_count,
                                                &tparams, &pkinds, &tparam_count);
            s_type_params = tparams;
            s_param_kinds = pkinds;
            s_type_param_count = tparam_count;
        } else if (impl_type_name) {
            // No method-level extension: the struct's params (already installed
            // as s_type_params by the impl-block loop) apply as-is.
            tparams = prev_tparams;
            pkinds = prev_pkinds;
            tparam_count = prev_tparam_count;
        }

        if (s_curr.type != TOK_LPAREN) parse_error("Expected '('");
        advance();

        ASTNode* node = new_node(AST_FUNC_DECL);
        node->func_decl.name = impl_type_name ? mname : name.start;
        node->func_decl.name_len = impl_type_name ? mname_len : name.length;
        node->func_decl.param_count = 0;
        node->func_decl.pack_param_index = -1; // packs not supported on impl methods yet
        size_t param_cap = 6;
        node->func_decl.param_syms = (Symbol**)calloc(param_cap, sizeof(Symbol*));

        // Temporarily set up a scope for the parameters
        SymbolTable* prev_table = s_symtable;
        s_symtable = SymTable_Create(prev_table);
        s_symtable->is_function_scope = true;

        if (impl_type_name) {
            // Inject self as first param (name "self"), type TYPE*.
            Type* self_base = (Type*)calloc(1, sizeof(Type));
            self_base->cls = TYPE_STRUCT;
            self_base->struct_name = impl_type_name;

            Type* self_type = (Type*)calloc(1, sizeof(Type));
            self_type->cls = TYPE_POINTER;
            self_type->pointer_base = self_base;

            Symbol* self_sym = SymTable_Add(s_symtable, "self", 4, self_type, SYM_LOCAL);
            node->func_decl.param_syms[node->func_decl.param_count++] = self_sym;
        }

        bool is_vararg = false;
        if (s_curr.type != TOK_RPAREN) {
            while (1) {
                if (s_curr.type == TOK_ELLIPSIS) {
                    is_vararg = true;
                    advance();
                    break;
                }
                if (node->func_decl.param_count >= param_cap) {
                    param_cap *= 2;
                    node->func_decl.param_syms = (Symbol**)realloc(node->func_decl.param_syms, param_cap * sizeof(Symbol*));
                }
                // Spec: parameters are `type name`, same order as declarations.
                Type* param_type = parse_type();
                if (!param_type) { parse_error("expected parameter type"); }

                // Prototype: `T... name` -- a variadic type-pack value-parameter.
                // Every trailing call argument bundles into ONE synthesized anon
                // struct bound to this slot at each call site (see types.c).
                bool is_pack_param = false;
                if (s_curr.type == TOK_ELLIPSIS) {
                    is_pack_param = true;
                    advance();
                    if (node->func_decl.pack_param_index != -1)
                        parse_error("at most one `T...` pack parameter is allowed per function");
                }

                if (s_curr.type != TOK_IDENTIFIER) parse_error("Expected parameter name");
                Token param_name = s_curr;
                advance();

                Symbol* psym = SymTable_Add(s_symtable, param_name.start, param_name.length, param_type, SYM_LOCAL);
                psym->is_pack = is_pack_param;
                node->func_decl.param_syms[node->func_decl.param_count] = psym;
                if (is_pack_param) node->func_decl.pack_param_index = (int)node->func_decl.param_count;
                node->func_decl.param_count++;
                
                if (s_curr.type == TOK_COMMA) {
                    if (is_pack_param) parse_error("a `T...` pack parameter must be the last parameter");
                    advance();
                } else {
                    break;
                }
            }
        }
        
        if (s_curr.type != TOK_RPAREN) parse_error("Expected ')'");
        advance();
        
        Type* ret_type = NULL;
        // An OMITTED return type. Two things have to be true to consume one here:
        //
        //   (a) a type actually begins at s_curr -- previously this asked only
        //       `!= TOK_LBRACE`, i.e. it consumed a type UNCONDITIONALLY unless a body
        //       followed. For an `extern` (which has no body at all) that meant the NEXT
        //       DECLARATION got eaten as the return type: `extern fn rel(u8* p)` followed
        //       by `struct S {...}` read `struct S` as rel's return type, because `struct`
        //       legitimately begins a type (an anonymous struct). token_begins_type was
        //       right; the caller simply never asked it.
        //
        //   (b) it is on the SAME LINE as the `)`. Even with (a), one token of lookahead
        //       cannot distinguish "no return type, next declaration follows" from "return
        //       type is whatever comes next" -- both are type-initial. A return type
        //       continues the signature, so it sits on the signature's line; a new
        //       declaration starts a new line. Torrent is newline-terminated (no `;`), so
        //       this is the same rule the rest of the grammar already reads by, and the
        //       lexer already tracks it.
        if (s_curr.type != TOK_LBRACE && !Lexer_NewlineBefore && curr_begins_type()) {
            ret_type = parse_type();
        }
        node->func_decl.return_type = ret_type;
        
        Type* ftype = (Type*)calloc(1, sizeof(Type));
        ftype->cls = TYPE_FUNCTION;
        ftype->function.return_type = ret_type;
        ftype->function.param_count = node->func_decl.param_count;
        ftype->function.is_vararg = is_vararg;
        ftype->function.param_types = (Type**)malloc(node->func_decl.param_count * sizeof(Type*));
        for (size_t i = 0; i < node->func_decl.param_count; i++) {
            ftype->function.param_types[i] = node->func_decl.param_syms[i]->type;
        }
        
        Symbol* func_sym = impl_type_name
            ? SymTable_Add(prev_table, mname, mname_len, ftype, SYM_FUNCTION)
            : SymTable_Add(prev_table, name.start, name.length, ftype, SYM_FUNCTION);
        node->func_decl.sym = func_sym;
        if (is_extern) func_sym->is_extern = true;
        if (is_pub) func_sym->is_pub = true;
        func_sym->has_init = true;
        func_sym->func_decl = node; // for constexpr interpretation of the body
        node->func_decl.type_params = tparams;
        node->func_decl.param_kinds = pkinds;
        node->func_decl.type_param_count = tparam_count;
        if (tparam_count > 0) {
            // Mark generic: not compiled at definition; the backend clones+substitutes
            // and compiles each [T] instantiation on demand. Stash the decl AST on the
            // symbol so instantiation sites can find it.
            func_sym->generic_decl = node;
        }
        func_sym->is_extern = is_extern;

        if (is_extern) {
            s_symtable = prev_table; // pop the parameter scope
            if (s_curr.type == TOK_SEMI) advance();
            return node;
        }
        
        if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' for function body");
        ASTNode* block = parse_braced_block(false, false);

        node->func_decl.body = block;
        s_symtable = prev_table;
        // Restore any enclosing type-param scope (we don't nest generic fns, but be safe).
        s_type_params = prev_tparams;
        s_param_kinds = prev_pkinds;
        s_type_param_count = prev_tparam_count;
        return node;
    } else {
        // Any other top-level construct (e.g. a global variable declaration or a
        // bare REPL expression) is a SINGLE statement. The two-pass file driver
        // calls Parse_Block() once per top-level unit, so this must consume
        // exactly one statement — not greedily loop to EOF, which would swallow
        // a following `fn main` definition and misparse it as a function type.
        return parse_statement();
    }
}

void Parse_Init(const char* filename, const char* source) {
    Lexer_Init(filename, source);
    if (!s_symtable) {
        s_symtable = SymTable_Create(NULL);
    }
    advance();
}

// ─── Pass 0b: signature-only pre-parse (see compiler.h) ──────────────────────
// Walk the top level, read each declaration's HEADER, skip its body. The whole point
// is the generic parameter list, and it is obtained by calling the SAME
// parse_generic_param_list the real parse calls -- no second copy of the
// type-vs-value rule exists anywhere.

// Generic FUNCTION headers found by pass 0b. A side table rather than symbols: pass 1
// adds the real symbol with SymTable_Add, which rejects duplicates, so a stub symbol
// would collide with the genuine declaration. The explicit-generic-call site consults
// this only when the symbol isn't visible yet -- i.e. exactly the forward-reference case.
typedef struct GenericSig GenericSig;
static GenericSig* s_gsigs = NULL;
static size_t s_gsig_count = 0, s_gsig_cap = 0;

static void gsig_register(const char* name, size_t len,
                          const char** tparams, Type** pkinds, size_t pcount) {
    for (size_t i = 0; i < s_gsig_count; i++)          // idempotent across files
        if (s_gsigs[i].name_len == len && strncmp(s_gsigs[i].name, name, len) == 0) return;
    if (s_gsig_count >= s_gsig_cap) {
        s_gsig_cap = s_gsig_cap ? s_gsig_cap * 2 : 16;
        s_gsigs = (GenericSig*)realloc(s_gsigs, s_gsig_cap * sizeof(GenericSig));
    }
    s_gsigs[s_gsig_count++] = (GenericSig){ name, len, tparams, pkinds, pcount };
}
static struct GenericSig* gsig_find(const char* name, size_t len) {
    for (size_t i = 0; i < s_gsig_count; i++)
        if (s_gsigs[i].name_len == len && strncmp(s_gsigs[i].name, name, len) == 0)
            return &s_gsigs[i];
    return NULL;
}

static void skip_braced_body(void) {
    // Skip to the matching '}', counting depth. Tolerates a decl with no body.
    while (s_curr.type != TOK_LBRACE && s_curr.type != TOK_EOF &&
           s_curr.type != TOK_STRUCT && s_curr.type != TOK_ENUM && s_curr.type != TOK_UNION &&
           s_curr.type != TOK_FN && s_curr.type != TOK_IMPL &&
           s_curr.type != TOK_EXTERN && s_curr.type != TOK_PUB &&
           s_curr.type != TOK_CONST && s_curr.type != TOK_ALIAS)
        advance();
    if (s_curr.type != TOK_LBRACE) return;   // no body (e.g. `extern fn`)
    int depth = 0;
    do {
        if (s_curr.type == TOK_LBRACE) depth++;
        else if (s_curr.type == TOK_RBRACE) depth--;
        advance();
    } while (depth > 0 && s_curr.type != TOK_EOF);
}

void Parse_Signatures(const char* filename, const char* source) {
    Lexer_Init(filename, source);
    if (!s_symtable) s_symtable = SymTable_Create(NULL);
    advance();

    if (setjmp(s_err_buf) != 0) {
        // A malformed header here is not fatal: Pass 1 re-parses everything and will
        // report the error properly, with the right message and position. Bail out of
        // the pre-pass quietly rather than double-reporting.
        return;
    }

    while (s_curr.type != TOK_EOF) {
        if (s_curr.type == TOK_PUB) { advance(); continue; }
        if (s_curr.type == TOK_EXTERN) { advance(); continue; }

        bool is_struct = (s_curr.type == TOK_STRUCT || s_curr.type == TOK_ENUM || s_curr.type == TOK_UNION);
        bool is_fn     = (s_curr.type == TOK_FN);
        if (!is_struct && !is_fn) { advance(); continue; }
        advance(); // 'struct' / 'enum' / 'fn'

        // `fn (` is a function-POINTER type, not a declaration -- no name follows.
        if (s_curr.type != TOK_IDENTIFIER) continue;
        Token name = s_curr;
        advance();

        if (s_curr.type != TOK_LBRACKET) { skip_braced_body(); continue; }  // not generic

        // The one thing this pass exists for.
        const char** prev_tp  = s_type_params;
        Type**       prev_pk  = s_param_kinds;
        size_t       prev_tpc = s_type_param_count;
        const char** tparams = NULL; Type** pkinds = NULL; size_t pcount = 0;
        parse_generic_param_list(&tparams, &pkinds, &pcount);
        s_type_params = prev_tp; s_param_kinds = prev_pk; s_type_param_count = prev_tpc;

        if (is_struct) {
            // Deliberately NOT recording struct kinds here.
            //
            // Pass 0a already gives structs their arity, and the remaining gap (a
            // forward use with an explicit VALUE arg, `Box[i32, 4]` above `struct
            // Box[T, u32 N]`) turns out not to be fixable this way: installing
            // type_params/param_kinds from THIS pass's arena leaves pass 1 re-parsing the
            // real declaration and installing different pointers, while instantiations
            // may already have cached against the pass-0b ones -- which silently produced
            // an empty layout (sizeof == 0) instead of an error. Trading a loud failure
            // for a silent wrong answer is the worst possible move (§1), so leave it.
            //
            // Functions have no such hazard: their kinds live in a side table that only
            // the call site reads, and pass 1 builds the real func_decl independently.
            skip_braced_body();
            continue;
        } else {
            // Record the function's generic header in a SIDE TABLE rather than adding a
            // Symbol. Pass 1 will add the real symbol via SymTable_Add, which rejects
            // duplicates -- so a stub symbol here would collide with the genuine
            // declaration and turn every generic fn into a "already declared" error.
            // The call site consults this table only as a fallback, when the symbol is
            // not yet visible (i.e. exactly the forward-reference case).
            gsig_register(name.start, name.length, tparams, pkinds, pcount);
        }
        skip_braced_body();
    }

    return NULL;
}


static bool s_parse_had_error = false;
bool Parse_HadError(void) { return s_parse_had_error; }

ASTNode* Parse_Block(void) {
    if (setjmp(s_err_buf) != 0) {
        s_parse_had_error = true; // distinguishes an error-NULL from a clean EOF-NULL
        return NULL;
    }

    if (s_curr.type == TOK_EOF) return NULL;

    return parse_top_level();
}

SymbolTable* Get_SymTable(void) {
    return s_symtable;
}