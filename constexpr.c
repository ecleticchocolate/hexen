#include "compiler.h"
#include "codegen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Named-constant registry (top-level, single namespace) ---

static ConstDef* s_consts = NULL;
static size_t s_const_count = 0;
static size_t s_const_capacity = 0;

// --- Globals needing byte-baking (see compiler.h) ---
// Keyed on "has folded bytes", NOT on "lives in the global scope table", so a
// const aggregate declared inside a function body is emitted correctly.
static Symbol** s_emit_globals   = NULL;
static size_t   s_emit_count     = 0;
static size_t   s_emit_capacity  = 0;

void Global_RegisterForEmit(Symbol* sym) {
    if (!sym) return;
    for (size_t i = 0; i < s_emit_count; i++)   // idempotent
        if (s_emit_globals[i] == sym) return;
    if (s_emit_count >= s_emit_capacity) {
        s_emit_capacity = s_emit_capacity ? s_emit_capacity * 2 : 8;
        s_emit_globals = (Symbol**)realloc(s_emit_globals, s_emit_capacity * sizeof(Symbol*));
    }
    s_emit_globals[s_emit_count++] = sym;
}
size_t  Global_EmitCount(void)      { return s_emit_count; }
Symbol* Global_EmitAt(size_t i)     { return (i < s_emit_count) ? s_emit_globals[i] : NULL; }

ConstDef* Const_GetAll(size_t* out_count) {
    if (out_count) *out_count = s_const_count;
    return s_consts;
}

ConstDef* Const_Find(const char* name, size_t len) {
    for (size_t i = 0; i < s_const_count; i++) {
        if (strlen(s_consts[i].name) == len &&
            strncmp(s_consts[i].name, name, len) == 0) {
            return &s_consts[i];
        }
    }
    return NULL;
}

ConstDef* Const_Register(const char* name, size_t len, int64_t value, Type* type) {
    if (s_const_count >= s_const_capacity) {
        s_const_capacity = s_const_capacity ? s_const_capacity * 2 : 8;
        s_consts = (ConstDef*)realloc(s_consts, s_const_capacity * sizeof(ConstDef));
    }
    ConstDef* c = &s_consts[s_const_count++];
    c->name = strndup(name, len);
    c->value = value;
    c->type = type;
    c->is_pub = false;
    c->pending_expr = NULL;
    return c;
}

// Use sites that inlined a pending const's (placeholder 0) value, to be patched
// once the const folds. A literal node + the const it came from.
typedef struct { ASTNode* node; ConstDef* cdef; } PendingUse;
static PendingUse s_pending_uses[1024];
static size_t s_pending_use_count = 0;

void Const_RegisterPendingUse(ASTNode* node, ConstDef* cdef) {
    if (s_pending_use_count < 1024)
        s_pending_uses[s_pending_use_count++] = (PendingUse){node, cdef};
}

// How many pending-const uses have been registered so far. The const-decl parser
// snapshots this around an initializer: if it grew, the initializer inlined a
// still-pending const (as a placeholder 0), so THIS const must also defer.
size_t Const_PendingUseCount(void) { return s_pending_use_count; }

// Retry const initializers that couldn't fold at parse time (forward-referenced a
// later fn). By now the whole file is parsed, so all functions are registered.
// Re-fold each pending const, then back-patch every use site that inlined its
// placeholder value.
bool Const_ResolvePending(void) {
    // Fixpoint: a pending const may depend on another pending const (`const B =
    // A * 2`). Keep resolving until a full pass makes no progress; then any still-
    // pending const is genuinely un-foldable.
    bool progress = true;
    while (progress) {
        progress = false;
        for (size_t i = 0; i < s_const_count; i++) {
            ConstDef* c = &s_consts[i];
            if (!c->pending_expr) continue;
            int64_t v;
            if (ConstEval(c->pending_expr, &v)) { c->value = v; c->pending_expr = NULL; progress = true; }
        }
    }
    for (size_t i = 0; i < s_const_count; i++) {
        if (s_consts[i].pending_expr) {
            fprintf(stderr, "Error: const '%s' initializer is not a constant expression\n", s_consts[i].name);
            return false;
        }
    }
    // Back-patch inlined use sites with the now-resolved values.
    for (size_t i = 0; i < s_pending_use_count; i++) {
        ASTNode* n = s_pending_uses[i].node;
        ConstDef* c = s_pending_uses[i].cdef;
        if (c->type && Type_IsFloat(c->type)) {
            n->lit_kind = LIT_FLOAT;
            double d; memcpy(&d, &c->value, sizeof d);
            n->float_value = d;
        } else {
            n->lit_kind = LIT_INT;
            n->int_value = (uint64_t)c->value;
        }
    }
    s_pending_use_count = 0;
    return true;
}

// --- Compile-time environment (params + block locals) ---
//
// The evaluator is a PURE TOTAL tree-walking interpreter over the no-runtime-
// dependency subset. To call functions (Option A: functions are constexpr-callable
// without marking; the context drives it) and to evaluate block locals (B1: a block
// produces a value), AST_IDENT must resolve not just global `const`s but also params
// bound for the current call and locals declared in the current block. A flat stack
// of (Symbol* -> value) frames does that; it is unwound after each call/block.
//
// WALL 1 (totality): a step budget bounds evaluation so a non-terminating constexpr
// function gives a clean error instead of hanging the compiler. WALL 2 (purity) and
// WALL 4 (no code emission) are enforced structurally: there is no I/O/mutation node
// here, and a nested `fn` definition inside a constexpr block is rejected.

// A comptime value is an int OR a float. Floats travel as their IEEE-754 bit
// pattern inside the same int64 slot (bitcast), with `is_float` recording the
// interpretation. This is the "richer value model" approach (à la Zig's one
// Value type) rather than special-casing floats per node — every eval sets both
// `*out` and the parallel s_ce_isfloat flag, so float-ness flows through binops,
// UNIFIED COMPTIME STORE. Every local/param has an ADDRESS in the one comptime
// arena; a binding is just {symbol, address, type}. Reading is load-at-addr,
// writing is store-at-addr, `&x` is the addr, a pointer is an addr, deref is load.
// This mirrors runtime, so scalars, aggregates and pointers share one model — an
// aggregate's "value" is its addr, so a struct and a pointer to it are the same
// thing. The only escape rule: an arena offset can't become a runtime pointer.
typedef struct { struct Symbol* sym; uint32_t addr; Type* type; } CEBinding;
static CEBinding s_ce_env[256];
static int s_ce_env_top = 0;

const char** s_ce_generic_params = NULL;
struct Type** s_ce_generic_args = NULL;
size_t s_ce_generic_n = 0;

static long s_ce_budget = 0; // remaining steps; <=0 => exceeded

#include "codegen.h"
#define CE_MEM_SIZE 65536
static uint8_t  s_ce_mem[CE_MEM_SIZE];
// Offset 0 is reserved and never allocated into: null is represented as the
// plain integer 0 (see AST_INT_LITERAL's LIT_NULL case below), so if a real
// allocation could ever land at offset 0, that pointer would be bit-for-bit
// indistinguishable from null — `if ptr == null` would misfire for the very
// first thing ever allocated on the comptime heap. CE_MEM_GUARD is well
// beyond the largest alignment ce_mem_alloc rounds up to (16), so no
// legitimate allocation's rounded start can land back on 0.
#define CE_MEM_GUARD 16
static uint32_t s_ce_mem_top = CE_MEM_GUARD;
// Bytes below this watermark are PERSISTENT const-aggregate storage that survives
// across ConstEval calls (so `const B = P.x` can read into `P`). Per-eval scratch
// (locals, temporaries) resets to this line, not to 0 (see CE_MEM_GUARD above).
static uint32_t s_ce_mem_persist = CE_MEM_GUARD;
// The last-produced value's "shape": is it an arena address pointing at an
// aggregate (struct/array)? Kept only so field/index know to treat the value as a
// base offset vs. a scalar. Not stored per-binding anymore — derived from type.
static bool     s_ce_isagg = false;

static int64_t ce_mem_alloc(uint64_t size) {
    uint32_t off = (s_ce_mem_top + 15u) & ~15u;
    if ((uint64_t)off + (size ? size : 1) > CE_MEM_SIZE) return -1;
    s_ce_mem_top = off + (uint32_t)(size ? size : 1);
    memset(s_ce_mem + off, 0, size ? size : 1);
    return (int64_t)off;
}
static void ce_mem_store(uint32_t off, int64_t v, int width) {
    if (off + (uint32_t)width <= CE_MEM_SIZE) memcpy(s_ce_mem + off, &v, width);
}
static int64_t ce_mem_load(uint32_t off, int width, bool sgn) {
    int64_t v = 0;
    if (off + (uint32_t)width <= CE_MEM_SIZE) memcpy(&v, s_ce_mem + off, width);
    if (width < 8) {
        if (sgn) { int64_t s = 1LL << (width*8 - 1); v = (v ^ s) - s; }
        else     { v &= (1LL << (width*8)) - 1; }
    }
    return v;
}

// Float-ness of the value most recently produced by ConstEval. Set by every
// node alongside *out; read by the consumer (binop dispatch, cast, the public
// wrapper that rejects a float where an int is required).
bool s_ce_isfloat = false;

// Function-symbol-ness of the value most recently produced by ConstEval: true
// when *out is not a real integer but a Symbol* (a SYM_FUNCTION), produced by
// folding a bare function name or a const-generic fn parameter to a value.
// Every consumer that turns this int64 into an AST_INT_LITERAL must check
// this and tag the literal LIT_FN_SYMBOL instead of LIT_INT — otherwise the
// backend has no way to tell the pointer apart from an ordinary constant and
// will emit it as raw bits, producing a "function pointer" that crashes the
// instant it's called (see clone_ast's AST_CONST_EXPR and AST_IDENT cases).
bool s_ce_isfnsym = false;

static double ce_bits_to_f(int64_t b) { double d; memcpy(&d, &b, sizeof d); return d; }
static int64_t ce_f_to_bits(double d) { int64_t b; memcpy(&b, &d, sizeof b); return b; }

// Narrow an f64 value (carried as int64 bits) to a 32-bit float bit pattern in
// the low 4 bytes — for storing into an f32 slot. Was hand-rolled (the fiddly
// (float) + memcpy dance) in 3 separate places; centralized so an f32 store can't
// silently get it wrong (that was the f32-field bug class).
static int64_t ce_f64bits_to_f32bits(int64_t f64bits) {
    double d; memcpy(&d, &f64bits, sizeof d);
    float f = (float)d;
    uint32_t fb; memcpy(&fb, &f, sizeof fb);
    return (int64_t)fb;
}

// Widen a 32-bit float bit pattern (low 4 bytes) back to f64 bits — for READING
// an f32 slot, since ConstEval carries every float as f64 bits. The mirror of
// ce_f64bits_to_f32bits; without it an f32 field read fed 32-bit bits to f64
// consumers = garbage.
static int64_t ce_f32bits_to_f64bits(int64_t f32bits) {
    uint32_t fb = (uint32_t)f32bits;
    float f; memcpy(&f, &fb, sizeof f);
    return ce_f_to_bits((double)f);
}

// THE single typed arena store/load pair (was open-coded per site; forgetting the
// f32 narrow/widen = the f32-field bug class, both directions).
static void ce_mem_store(uint32_t off, int64_t v, int width); // fwd
static int64_t ce_mem_load(uint32_t off, int width, bool sgn); // fwd
static void ce_mem_store_typed(uint32_t off, int64_t v, Type* t) {
    if (Type_IsFloat(t) && Type_Width(t) == 4) v = ce_f64bits_to_f32bits(v);
    ce_mem_store(off, v, Type_Width(t));
}
static int64_t ce_mem_load_typed(uint32_t off, Type* t) {
    int64_t v = ce_mem_load(off, Type_Width(t), Type_IsSigned(t));
    if (Type_IsFloat(t) && Type_Width(t) == 4) v = ce_f32bits_to_f64bits(v);
    return v;
}

static bool ConstEval_inner(ASTNode* node, int64_t* out); // body; ConstEval wraps it w/ budget

// Set true when a `return` fires during evaluation, so it propagates up through
// nested blocks/ifs and halts the enclosing function body (not just the literal
// block the `return` sits in). Cleared when a call frame consumes it.
static bool s_ce_returned = false;
// break/continue signals inside a comptime loop (consumed by the loop).
static bool s_ce_broke = false;
static bool s_ce_continued = false;

// The return type of the comptime function currently being evaluated, so a bare
// `{...}`/`.Variant` in return position can be resolved against it (const runs at
// parse time, before typecheck would have resolved it). Saved/restored per frame.
static Type* s_ce_ret_type = NULL;

// Find a binding slot for a symbol (innermost shadow wins).
static CEBinding* ce_find(struct Symbol* sym) {
    for (int i = s_ce_env_top - 1; i >= 0; i--)
        if (s_ce_env[i].sym == sym) return &s_ce_env[i];
    return NULL;
}

// Read a local's value: an aggregate's value IS its address; a scalar's is the
// bytes loaded at its address. The one definition of "a variable's value".
static bool ce_lookup_local(struct Symbol* sym, int64_t* out) {
    CEBinding* b = ce_find(sym);
    if (!b) return false;
    if (Type_IsAggregate(b->type)) {
        *out = (int64_t)b->addr;          // aggregate: value is its address
        s_ce_isagg = true; s_ce_isfloat = false;
    } else {
        *out = ce_mem_load_typed(b->addr, b->type);
        s_ce_isagg = false;
        s_ce_isfloat = Type_IsFloat(b->type);
    }
    return true;
}

// Write a scalar local: store into its arena slot. (Aggregate wholesale
// reassignment isn't a scalar store; those go through the aggregate copy path.)
static bool ce_set_local(struct Symbol* sym, int64_t val, bool is_float) {
    (void)is_float;
    CEBinding* b = ce_find(sym);
    if (!b) return false;
    // The one real caller (AST_ASSIGN) now handles the aggregate case itself
    // (a proper byte copy into b's existing storage) before ever reaching
    // here. If this still gets an aggregate target, something upstream
    // disagrees about the type — fail the fold rather than silently
    // rebinding b's address to val, which is the aliasing bug this function
    // used to have (b and val's source would become the same arena bytes
    // under two names, so mutating one would corrupt the other).
    if (Type_IsAggregate(b->type)) return false;
    ce_mem_store_typed(b->addr, val, b->type);
    return true;
}

// Bind a NEW local. Aggregate with init_is_addr: adopt the arena offset its bytes
// already live at (literal/return), so &x/x.f/mutation hit the live bytes. Scalar:
// allocate a slot and store the initial value.
static bool ce_bind_local(struct Symbol* sym, Type* t, int64_t init_val, bool init_is_addr) {
    if (s_ce_env_top >= 256) return false;
    uint32_t addr;
    if (Type_IsAggregate(t)) {
        addr = init_is_addr ? (uint32_t)init_val : (uint32_t)ce_mem_alloc(Type_SizeOf(t));
    } else {
        int64_t a = ce_mem_alloc(Type_SizeOf(t) ? Type_SizeOf(t) : 8);
        if (a < 0) return false;
        addr = (uint32_t)a;
        ce_mem_store_typed(addr, init_val, t);
    }
    s_ce_env[s_ce_env_top].sym  = sym;
    s_ce_env[s_ce_env_top].addr = addr;
    s_ce_env[s_ce_env_top].type = t;
    s_ce_env_top++;
    return true;
}

// ── constexpr LayoutSink ──────────────────────────────────────────────────────
// The arena sink for layout_fill: writes aggregate-literal bytes into s_ce_mem.
// The "base" is a static offset tracked on a small stack (the constexpr analogue
// of the backend pushing a runtime base address).
static uint32_t s_ce_base_stack[64];
static int      s_ce_base_top = 0;
static uint32_t ce_base(void) { return s_ce_base_top ? s_ce_base_stack[s_ce_base_top - 1] : 0; }

static bool ce_sink_put_scalar(void* ctx, uint64_t off, int width, bool is_float, ASTNode* val) {
    (void)ctx;
    int64_t v; if (!ConstEval(val, &v)) return false;
    // Float carried as f64 bits; an f32 slot (width 4) needs the 32-bit pattern.
    if (is_float && width == 4) v = ce_f64bits_to_f32bits(v);
    ce_mem_store(ce_base() + (uint32_t)off, v, width);
    return true;
}
static bool ce_sink_put_default(void* ctx, uint64_t off, uint64_t size, uint8_t* bytes) {
    (void)ctx;
    if (ce_base() + off + size > CE_MEM_SIZE) return false;
    memcpy(s_ce_mem + ce_base() + (uint32_t)off, bytes, size);
    return true;
}
static bool ce_sink_put_agg_value(void* ctx, uint64_t off, uint64_t size, ASTNode* val) {
    (void)ctx;
    int64_t src; if (!ConstEval(val, &src)) return false;  // yields an arena offset
    if (!s_ce_isagg) return false;
    memcpy(s_ce_mem + ce_base() + (uint32_t)off, s_ce_mem + src, size);
    return true;
}
static bool ce_sink_put_tag(void* ctx, int tag) {
    (void)ctx; ce_mem_store(ce_base(), (int64_t)tag, 4); return true;
}
static bool ce_sink_enter_sub(void* ctx, uint64_t off) {
    (void)ctx;
    if (s_ce_base_top >= 64) return false;
    s_ce_base_stack[s_ce_base_top++] = ce_base() + (uint32_t)off;
    return true;
}
static void ce_sink_leave_sub(void* ctx) { (void)ctx; if (s_ce_base_top) s_ce_base_top--; }

static const LayoutSink s_ce_sink = {
    .ctx = NULL,
    .put_scalar = ce_sink_put_scalar,
    .put_default = ce_sink_put_default,
    .put_agg_value = ce_sink_put_agg_value,
    .put_tag = ce_sink_put_tag,
    .enter_sub = ce_sink_enter_sub,
    .leave_sub = ce_sink_leave_sub,
};

// Materialize an aggregate literal into the arena; returns its offset (-1 fail).
static int64_t ce_build_aggregate(ASTNode* lit, Type* t) {
    // const is evaluated at PARSE time, before typecheck runs resolve_brace_literal
    // on the literal — so its sdef/elem_type may be NULL here. Resolve it against
    // the declared type now so layout_fill can read field offsets. (Idempotent if
    // already resolved.)
    resolve_brace_literal(lit, t);
    int64_t off = ce_mem_alloc(Type_SizeOf(t));
    if (off < 0) return -1;
    s_ce_base_stack[s_ce_base_top++] = (uint32_t)off;
    bool ok = layout_fill(lit, &s_ce_sink);
    s_ce_base_top--;
    return ok ? off : -1;
}

// LIFTED: materialize an aggregate const into PERSISTENT arena storage and return
// its offset, so later consts can address into it (`&P`, `P.x`, `P[i]`). Returns
// -1 on failure. The offset is stable for the rest of compilation.
// Copy `size` bytes out of the persistent comptime arena at `off` (produced by
// ConstEval_AggPersist) into out_buf — for emitting a const aggregate's runtime
// image without re-evaluating its initializer.
bool ConstEval_ReadBytes(uint32_t off, uint8_t* out_buf, uint64_t size) {
    if ((uint64_t)off + size > CE_MEM_SIZE) return false;
    memcpy(out_buf, s_ce_mem + off, size);
    return true;
}

// ESCAPE-BOUNDARY CHECK. A const aggregate's bytes become part of the runtime
// image, but a pointer built during comptime holds an ARENA offset that has no
// meaning at runtime (the arena doesn't exist then). So a const may not store a
// live (non-null) pointer into comptime storage. Recursively scan the type over
// the folded bytes; return true if a non-null pointer field is found (= escape).
// null pointers are fine (0 is 0 at runtime too), as are pure-value aggregates.
bool ConstEval_AggHasEscapingPtr(Type* t, uint8_t* bytes, uint64_t size) {
    if (!t) return false;
    if (t->cls == TYPE_POINTER) {
        uint64_t p = 0;
        memcpy(&p, bytes, size < 8 ? size : 8);
        return p != 0;   // a non-null comptime pointer can't survive to runtime
    }
    if (t->cls == TYPE_STRUCT) {
        StructDef* sd = Struct_Find(t->struct_name);
        if (!sd) return false;
        for (size_t i = 0; i < sd->field_count; i++) {
            StructField* f = &sd->fields[i];
            if (f->offset >= size) continue;
            if (ConstEval_AggHasEscapingPtr(f->type, bytes + f->offset, size - f->offset))
                return true;
        }
        return false;
    }
    if (t->cls == TYPE_ARRAY) {
        Type* et = t->array.element;
        uint64_t esz = Type_SizeOf(et);
        if (!esz) return false;
        uint64_t n = size / esz;
        for (uint64_t i = 0; i < n; i++)
            if (ConstEval_AggHasEscapingPtr(et, bytes + i * esz, esz)) return true;
        return false;
    }
    return false;
}

int64_t ConstEval_AggPersist(ASTNode* node, Type* t) {
    // Materialize an aggregate const into PERSISTENT arena storage; return its
    // offset. A literal builds directly into the persist region (fast path). Any
    // other aggregate-valued expression — notably a fn call that returns a struct/
    // array, i.e. a whole computation run at compile time — is evaluated to a
    // scratch offset, then its bytes are copied down into the persist region.
    bool is_literal = (node->type == AST_STRUCT_LITERAL || node->type == AST_ARRAY_LITERAL);
    if (is_literal) {
        // Prime the budget frame so any nested field ConstEval during layout_fill
        // is NOT treated as a fresh top frame — otherwise it would reset
        // s_ce_mem_top back to persist mid-build and clobber the slot we just
        // allocated (two persisted aggregates would collide at offset 0).
        bool own_budget = (s_ce_budget == 0);
        if (own_budget) s_ce_budget = 100000;
        uint32_t save_top = s_ce_mem_top;
        s_ce_mem_top = s_ce_mem_persist;
        s_ce_isagg = false; s_ce_isfloat = false;
        int64_t off = ce_build_aggregate(node, t);
        if (off < 0) { s_ce_mem_top = save_top; if (own_budget) s_ce_budget = 0; return -1; }
        s_ce_mem_persist = s_ce_mem_top;   // commit
        if (own_budget) s_ce_budget = 0;
        return off;
    }
    // Non-literal: reserve the persistent slot FIRST (so the eval that follows
    // can't reclaim it), then evaluate into scratch above it, then copy down.
    uint32_t reserved = s_ce_mem_persist;
    uint64_t sz = Type_SizeOf(t);
    uint32_t after = (reserved + 15u) & ~15u;
    if ((uint64_t)after + (sz ? sz : 1) > CE_MEM_SIZE) return -1;
    s_ce_mem_persist = after + (uint32_t)(sz ? sz : 1);  // claim the dst slot
    int64_t v;
    if (!ConstEval(node, &v)) { s_ce_mem_persist = reserved; return -1; }
    if (!s_ce_isagg) { s_ce_mem_persist = reserved; return -1; }
    if ((uint64_t)v + sz <= CE_MEM_SIZE)
        memmove(s_ce_mem + after, s_ce_mem + v, sz);
    return (int64_t)after;
}

bool ConstEval(ASTNode* node, int64_t* out) {
    if (!node) return false;
    // Prime the step budget at the outermost entry so EVERY constexpr (not just
    // ones that call a function) is bounded — an inline `const X = while...` loop
    // must not be able to hang the compiler. Reset to 0 when this top frame exits.
    bool top_budget = (s_ce_budget == 0);
    if (top_budget) { s_ce_budget = 100000; s_ce_isfloat = false; s_ce_isagg = false; s_ce_mem_top = s_ce_mem_persist;
                      s_ce_returned = false; s_ce_broke = false; s_ce_continued = false; }
    s_ce_isagg = false; // most values are scalars; aggregate paths set it true
    s_ce_isfnsym = false; // most values are ordinary integers; fn-symbol paths set it true
    if (s_ce_budget > 0 && --s_ce_budget <= 0) {
        if (top_budget) {
            fprintf(stderr, "Error: constexpr evaluation exceeded step budget (possible infinite loop/recursion)\n");
            s_ce_budget = 0;
        }
        return false;
    }
    bool _r = ConstEval_inner(node, out);
    if (top_budget) s_ce_budget = 0;
    return _r;
}

// Fold an AGGREGATE constant expression and copy its raw bytes into `out_buf`
// (`size` bytes). Used for aggregate globals (`u32[3] g = {1,2,3}`) — now
// possible because constexpr builds aggregates into the arena. Returns false if
// the expr isn't a foldable closed-term aggregate.
bool ConstEval_Bytes(ASTNode* node, uint8_t* out_buf, uint64_t size) {
    int64_t v;
    if (!ConstEval(node, &v)) return false;
    if (s_ce_isagg) {
        // Aggregate: v is an arena offset; copy bytes directly from the arena.
        if ((uint64_t)v + size > CE_MEM_SIZE) return false;
        memcpy(out_buf, s_ce_mem + v, size);
    } else {
        // Scalar: v is the value itself (int or float bits). Write into the buffer
        // with the correct width; zero-extend for sizes larger than 8 bytes (not
        // currently possible for primitives, but defensive).
        if (size > 8) return false;
        if (s_ce_isfloat && size == 4) {
            // f64 bits -> f32 bits
            v = ce_f64bits_to_f32bits(v);
        }
        memcpy(out_buf, &v, size);
    }
    return true;
}

// Compute the comptime ARENA ADDRESS (offset) of an lvalue, plus its type. The
// generic address-of machinery: an lvalue is a field/index chain rooted in a
// comptime aggregate, so its address is base_offset + field/element offset. This
// is the ONE place lvalue addressing lives — used by `&x` (AST_ADDR) and by
// aggregate-element assignment (`a[i]=v`, `s.f=v`), instead of each hand-rolling
// it. The result is an internal arena offset; it can deref back into the arena
// but cannot escape (the const/global escape boundary forbids a pointer result),
// which is the "scope it cleverly" safety — no special-casing per node needed.
// When a comptime aggregate value arrives via a pointer (`&P`, or a `T*` bound to
// arena offset), its type is `T*` but its value is the aggregate's arena offset.
// For layout (field offsets, element size) we want T, not T*. One normalizer that
// read/write/address-of all share, so no path special-cases pointers.
static Type* ce_layout_type(Type* t) {
    if (t && t->cls == TYPE_POINTER && t->pointer_base) return t->pointer_base;
    return t;
}


// A base is addressable when it's a materialized aggregate (s_ce_isagg) or a
// pointer value (its int IS an arena address). Unifies s.f/s[i] with p.f/p[i].
static bool ce_base_is_addressable(ASTNode* base_node) {
    if (s_ce_isagg) return true;
    Type* t = Type_Infer(base_node);
    return t && t->cls == TYPE_POINTER;
}

static bool ce_lvalue_addr(ASTNode* node, uint32_t* out_addr, Type** out_type) {
    if (!node) return false;
    if (node->type == AST_INDEX) {
        int64_t base; if (!ConstEval(node->index.base, &base)) return false;
        if (!ce_base_is_addressable(node->index.base)) return false;
        Type* bt = ce_layout_type(Type_Infer(node->index.base));
        Type* et = bt ? (bt->cls == TYPE_ARRAY ? bt->array.element : bt) : NULL;
        if (!et) return false;
        int64_t idx; if (!ConstEval(node->index.index, &idx)) return false;
        *out_addr = (uint32_t)(base + idx * (int64_t)Type_SizeOf(et));
        *out_type = et;
        return true;
    }
    if (node->type == AST_FIELD) {
        int64_t base; if (!ConstEval(node->field.base, &base)) return false;
        if (!ce_base_is_addressable(node->field.base)) return false;
        Type* bt = ce_layout_type(Type_Infer(node->field.base));
        StructDef* sd = bt ? Struct_Find(bt->struct_name) : NULL;
        if (!sd) return false;
        StructField* f = Struct_FindField(sd, node->field.field_name, node->field.field_name_len);
        if (!f) return false;
        *out_addr = (uint32_t)(base + f->offset);
        *out_type = f->type;
        return true;
    }
    if (node->type == AST_IDENT) {
        // &local: every local now has an arena slot, so its address is just the
        // binding's addr — works uniformly for scalars AND aggregates. (An
        // aggregate's value already IS its address; a scalar's address is its slot.)
        CEBinding* b = node->ident.sym ? ce_find(node->ident.sym) : NULL;
        if (b) { *out_addr = b->addr; *out_type = b->type; return true; }
        // Not a local: maybe a const global aggregate (addressable in the arena).
        int64_t v; if (!ConstEval(node, &v)) return false;
        if (!s_ce_isagg) return false;
        *out_addr = (uint32_t)v;
        *out_type = Type_Infer(node);
        return true;
    }
    if (node->type == AST_DEREF) {
        // *p: a comptime pointer's VALUE already IS an arena offset (same
        // representation as every other value here — see the AST_IDENT case
        // above and ce_eval_assign's scalar-store comment), so the pointee's
        // address is just whatever the pointer operand evaluates to. This is
        // the same "evaluate the base, then address relative to it" shape as
        // the AST_INDEX/AST_FIELD cases above, just with no offset to add —
        // letting *p, *p.field, *p[i], and p.field[i]->... all compose through
        // ordinary recursion (each level's `base` comes from ConstEval-ing the
        // inner expression) instead of needing a dedicated chain-walker.
        int64_t addr; if (!ConstEval(node->unary, &addr)) return false;
        Type* bt = Type_Infer(node->unary);
        if (!bt || bt->cls != TYPE_POINTER || !bt->pointer_base) return false;
        *out_addr = (uint32_t)addr;
        *out_type = bt->pointer_base;
        return true;
    }
    return false;
}

// ─── constexpr AggBinopSink ────────────────────────────────────────────────
// Drives the shared agg_binop_apply (codegen.c) so aggregate ==,!=,+,-,*,&,|,^
// fold correctly at compile time -- byte comparison / real lanewise arithmetic
// on the arena, not the old bug of comparing two operands' arena OFFSETS as if
// they were the values themselves. Mirrors backend_x64.c's x86 sink one level
// up: same op set (enforced once, in agg_binop_apply), different leaf action.
typedef struct {
    uint32_t base_l, base_r; // arena offsets of the two operands
    uint32_t dst;            // arena offset for arithmetic/bitwise results (unused for eq)
    uint64_t eq_diff;        // OR-accumulator of byte differences, for EQ/NEQ
} CEAggBinopCtx;

static bool ce_agg_do_lane(void* ctxp, ASTNodeType op, uint64_t offset, int width, bool is_eq) {
    CEAggBinopCtx* c = (CEAggBinopCtx*)ctxp;
    if (c->base_l + offset + width > CE_MEM_SIZE || c->base_r + offset + width > CE_MEM_SIZE)
        return false;
    uint64_t lv = 0, rv = 0;
    memcpy(&lv, s_ce_mem + c->base_l + offset, width);
    memcpy(&rv, s_ce_mem + c->base_r + offset, width);
    c->eq_diff |= (lv ^ rv);
    return true;
}

static bool ce_agg_finish_eq(void* ctxp, bool is_eq, int64_t* out_bool) {
    CEAggBinopCtx* c = (CEAggBinopCtx*)ctxp;
    bool bytes_equal = (c->eq_diff == 0);
    *out_bool = is_eq ? bytes_equal : !bytes_equal;
    return true;
}

// Fold an aggregate binop (EQ/NEQ/ADD/SUB/MUL/BIT_AND/BIT_OR/BIT_XOR) whose
// operands are already-materialized arena values (offsets loff/roff, type t).
// Returns false if the op isn't aggregate-legal (caller treats that as "not a
// constant expression", same as any other ConstEval failure).
static bool ce_agg_binop(ASTNodeType op, Type* t, uint32_t loff, uint32_t roff, int64_t* out, bool* out_is_agg) {
    bool is_eq = (op == AST_EQ || op == AST_NEQ);
    CEAggBinopCtx ctx = { .base_l = loff, .base_r = roff, .dst = 0, .eq_diff = 0 };
    if (!is_eq) {
        int64_t dst = ce_mem_alloc(Type_SizeOf(t));
        if (dst < 0) return false;
        ctx.dst = (uint32_t)dst;
    }
    AggBinopSink sink = { .ctx = &ctx, .do_lane = ce_agg_do_lane, .finish_eq = ce_agg_finish_eq };
    if (!agg_binop_apply(op, t, t, &sink)) return false;
    if (is_eq) {
        *out = ctx.eq_diff == 0 ? (op == AST_EQ) : (op == AST_NEQ);
        *out_is_agg = false;
    } else {
        *out = (int64_t)ctx.dst;
        *out_is_agg = true;
    }
    return true;
}

static bool ce_eval_call(ASTNode* node, int64_t* out) {
            // Compile-time function call (Option A). The callee must be an ordinary
            // function whose AST we can interpret. Fold each argument, bind them to the
            // params in a fresh frame, evaluate the body, capture the `return` value.
            // c.method(args) parses as target_expr=AST_FIELD here (const folds before
            // typecheck). Run runtime's rewrite -> Base_method(&c, args) so methods
            // fold like any fn. Idempotent.
            if (node->call.target_expr && node->call.target_expr->type == AST_FIELD)
                try_rewrite_method_call(node);
            struct Symbol* fsym = node->call.sym;
            // Late resolution: a const that forward-referenced this fn was parsed
            // before the fn existed, so call.sym is NULL. By deferred-eval time the
            // fn is registered — look it up by name. (Enables const-before-fn.)
            if (!fsym && node->call.target_name)
                fsym = node->call.sym = SymTable_Find(Get_SymTable(),
                                                      node->call.target_name,
                                                      node->call.target_name_len);
            // Indirect call through a fn-ptr VALUE: target_expr is some expression
            // (ident/field/index/whatever) whose static type is a function or
            // pointer-to-function, but it isn't the method-call shape above (or
            // that rewrite didn't apply — e.g. a real callback FIELD, not a
            // mangled impl method). Fold target_expr like any other value; a
            // well-typed fn-valued expression can only ever have originated from
            // a bare function-ident fold (see AST_IDENT), which yields the
            // Symbol* itself as the value — so reinterpret it as one. Resolved
            // fresh on EVERY call (never cached onto node->call.sym): the whole
            // point of a fn-ptr param is that it can name a different function
            // on each invocation of the same call-site AST.
            if (!fsym && node->call.target_expr) {
                Type* tt = Type_Infer(node->call.target_expr);
                bool is_fnval = tt && (tt->cls == TYPE_FUNCTION ||
                                       (tt->cls == TYPE_POINTER && tt->pointer_base &&
                                        tt->pointer_base->cls == TYPE_FUNCTION));
                if (is_fnval) {
                    int64_t fv;
                    if (ConstEval(node->call.target_expr, &fv) && fv) {
                        struct Symbol* candidate = (struct Symbol*)(intptr_t)fv;
                        if (candidate->kind == SYM_FUNCTION) fsym = candidate;
                    }
                } else if (try_rewrite_call_operator(node, tt)) {
                    // Call-operator overload (`a(x)` where `a` is a struct
                    // value defining __call, not a function) -- same
                    // Type_Infer/ConstEval split every other operator dispatch
                    // needs here: the rewrite mutates `node` into a real
                    // method-call AST_CALL, so re-run this same function on it.
                    try_rewrite_method_call(node);
                    return ce_eval_call(node, out);
                }
            }
            if (!fsym || fsym->kind != SYM_FUNCTION) return false;
            ASTNode* fn = fsym->func_decl;
            // Generic call/method: monomorphize via the SAME machinery the JIT/AOT
            // path uses (Generic_Instantiate -> clone_ast), rather than
            // refusing outright. clone_ast is pure AST substitution with no codegen
            // dependency, so nothing here is JIT-specific — it was only ever gated
            // off because nothing had wired the comptime call site up to it yet.
            //
            // Type args: prefer node->call.type_args if typecheck's infer_generic
            // already ran and populated them (rare here, since const folds BEFORE
            // typecheck); otherwise fall back to self_type_args, which
            // try_rewrite_method_call already seeds from the receiver struct's own
            // instantiation (Box[i32,4].push(v) -> self_type_args = [i32, 4]).
            // This covers every type param being fixed by the struct — the common
            // case, and the only one reachable without also running infer_generic's
            // full bottom-up-from-arguments inference, which this path doesn't
            // attempt; a method declaring EXTRA type params of its own beyond the
            // struct's own is not yet foldable (falls through to fsym->generic_decl
            // still being unresolved, so the call fails cleanly rather than
            // silently instantiating with too few args).
            if (fsym->generic_decl) {
                Type** targs = node->call.type_args;
                size_t targc = node->call.type_arg_count;
                if (targc == 0) { targs = node->call.self_type_args; targc = node->call.self_type_arg_count; }
                
                if (targc > 0 && s_ce_generic_n > 0) {
                    Type** sub_targs = malloc(targc * sizeof(Type*));
                    for (size_t i = 0; i < targc; i++) {
                        sub_targs[i] = Type_Substitute(targs[i], s_ce_generic_params, s_ce_generic_args, s_ce_generic_n);
                    }
                    targs = sub_targs;
                }
                
                // Neither explicit type_args nor a struct-receiver's
                // self_type_args are populated (a bare `ident(42)`-style
                // call with T left for the compiler to work out). Run the
                // SAME bottom-up-from-arguments/top-down-from-target
                // inference the ordinary typecheck pass uses instead of
                // giving up — infer_generic only reads already-inferrable
                // argument types and the callee's static signature, and
                // only writes back onto this call node, so it's safe to
                // invoke standalone here even though const-folding runs
                // before the real typecheck pass. On success it populates
                // node->call.type_args/type_arg_count itself.
                if (targc == 0 && fsym->type && fsym->type->cls == TYPE_FUNCTION) {
                    infer_generic(node, NULL);
                    targs = node->call.type_args;
                    targc = node->call.type_arg_count;
                }
                ASTNode* gdecl = fsym->generic_decl;
                if (targc == 0 || targc != gdecl->func_decl.type_param_count) return false;
                struct Symbol* isym = Generic_Instantiate(fsym, targs, targc);
                if (!isym || !isym->func_decl) return false;
                fsym = isym;
                fn = isym->func_decl;
            }
            if (!fn) return false;
            if (fn->func_decl.type_param_count > 0) return false; // still generic after the above: not foldable
            if (node->call.arg_count != fn->func_decl.param_count) return false;

            // Start the step budget on the OUTERMOST comptime call so the bound covers
            // the whole evaluation (nested calls share it). 100k steps is plenty for
            // real const computation and still fails fast on runaway recursion.
            bool top = (s_ce_budget == 0);
            if (top) s_ce_budget = 100000;

            // Fold args BEFORE pushing the frame (they evaluate in the caller's scope).
            int64_t argv[8];
            if (fn->func_decl.param_count > 8) { if (top) s_ce_budget = 0; return false; }
            for (size_t i = 0; i < node->call.arg_count; i++) {
                // An arg gets its type from the parameter (const runs at parse time,
                // before typecheck would resolve it). resolve_brace_literal handles
                // both bare `{...}`/`.Variant` AND an int literal -> float rewrite
                // (so `mk(1)` for an f64 param folds 1.0, not integer bits).
                ASTNode* arg = node->call.args[i];
                if (i < fn->func_decl.param_count && fn->func_decl.param_syms[i] &&
                    (arg->type == AST_STRUCT_LITERAL || arg->type == AST_ARRAY_LITERAL ||
                     (arg->type == AST_INT_LITERAL && arg->lit_kind == LIT_INT)))
                    resolve_brace_literal(arg, fn->func_decl.param_syms[i]->type);
                if (!ConstEval(arg, &argv[i])) { if (top) s_ce_budget = 0; return false; }
            }
            int frame_base = s_ce_env_top;
            if (frame_base + (int)fn->func_decl.param_count > 256) { if (top) s_ce_budget = 0; return false; }
            for (size_t i = 0; i < fn->func_decl.param_count; i++) {
                struct Symbol* psym = fn->func_decl.param_syms[i];
                Type* pt = psym ? psym->type : NULL;
                // One rule per param: aggregate arg's value IS its arena address, so
                // the param aliases the caller's bytes (by-ref); scalar/pointer gets a
                // fresh slot holding its value. ce_bind_local does both.
                if (!ce_bind_local(psym, pt, argv[i], /*init_is_addr=*/Type_IsAggregate(pt))) {
                    s_ce_env_top = frame_base; if (top) s_ce_budget = 0; return false;
                }
            }
            Type* prev_ret = s_ce_ret_type;
            s_ce_ret_type = fn->func_decl.return_type;
            bool ok = ConstEval(fn->func_decl.body, out);
            s_ce_ret_type = prev_ret;
            bool returned = s_ce_returned;
            s_ce_returned = false;       // this frame consumes its return
            s_ce_env_top = frame_base;   // unwind the frame
            if (top) s_ce_budget = 0;
            // Non-void fn must return a value. A void fn is called for EFFECT: it may
            // mutate through pointer params into the caller's storage (this is what
            // makes mutating methods like c.inc() fold), succeeding with no value.
            Type* rt = fn->func_decl.return_type;
            bool is_void = (!rt) ||
                           (rt->cls == TYPE_PRIMITIVE &&
                            (rt->primitive == PRIM_VOID || rt->primitive == PRIM_V));
            if (is_void) { *out = 0; s_ce_isagg = false; s_ce_isfloat = false; return ok; }
            return ok && returned;
}

static bool ce_eval_assign(ASTNode* node, int64_t* out) {
            // Mutate an existing comptime local: `x = expr`. The value also
            // becomes the expression's result, matching runtime semantics.
            // Aggregate-element/field/deref assignment: `a[i] = v`, `s.f = v`, or
            // `*p = v`. Compute the target's arena address (offset), then store.
            // Still closed-term — we're writing into our own comptime arena, no
            // external memory (a comptime pointer's value already IS an arena
            // offset, so *p never touches anything outside this evaluator's own
            // heap, escaping or not — see ce_lvalue_addr's AST_DEREF case).
            ASTNode* lhs = node->binary.left;
            if (lhs && (lhs->type == AST_INDEX || lhs->type == AST_FIELD || lhs->type == AST_DEREF)) {
                // `a[i] = v` / `s.f = v` / `*p = v`: address via the shared lvalue-addr helper,
                // then store. (Same machinery as `&lvalue`.) Previously scalar-only
                // (elemty->cls != TYPE_PRIMITIVE bailed the whole fold); generalized
                // to pointer and aggregate targets too, since neither the address
                // computation (ce_lvalue_addr) nor a comptime pointer's representation
                // (already just an int64_t arena offset, same as every other value
                // here) needed anything new — only this store site was scalar-only.
                //
                // A bare struct/array literal RHS (`s.f = {.a=1, .b=2}`) has no
                // sdef/elem_type yet — const folding runs at PARSE time, before
                // Typecheck_Tree's own resolve_brace_literal call (types.c, the
                // AST_ASSIGN case) would otherwise have resolved it against the
                // LHS's type. Every other site in this file that can reach an
                // unresolved literal (call args, return, `new`) does this same
                // resolve first; this is that same requirement at one more site.
                // Type_Infer on the LHS is a pure type query — no evaluation, so
                // it doesn't disturb the RHS-before-LHS-address ordering below.
                if (node->binary.right->type == AST_STRUCT_LITERAL ||
                    node->binary.right->type == AST_ARRAY_LITERAL) {
                    Type* lhs_t = Type_Infer(lhs);
                    if (lhs_t) resolve_brace_literal(node->binary.right, lhs_t);
                }
                // Evaluation order: RHS first, then the LHS address — matches the
                // runtime backend's AST_ASSIGN codegen (compile_node_ctx on the RHS
                // before compile_lvalue on the LHS) so a side-effecting index/field
                // expression like `arr[f()] = g()` folds with the same observable
                // order comptime would have had if it ran at runtime instead.
                //
                // We don't yet know elemty (need ce_lvalue_addr for that), so the
                // RHS is evaluated generically first into rv/is_agg, and the
                // aggregate-vs-scalar/pointer dispatch happens after both sides
                // are known.
                int64_t rv; if (!ConstEval(node->binary.right, &rv)) return false;
                bool rhs_is_agg = s_ce_isagg;
                uint32_t addr; Type* elemty;
                if (!ce_lvalue_addr(lhs, &addr, &elemty)) return false;
                if (!elemty) return false;
                if (Type_IsAggregate(elemty)) {
                    // Whole-aggregate field/element assignment: rv is the RHS's
                    // arena address (its value IS its address); copy its bytes over
                    // the target's storage, same as construction-time ce_sink_put_agg_value.
                    if (!rhs_is_agg) return false;
                    uint64_t sz = Type_SizeOf(elemty);
                    if ((uint64_t)addr + sz > CE_MEM_SIZE || (uint64_t)rv + sz > CE_MEM_SIZE) return false;
                    memcpy(s_ce_mem + addr, s_ce_mem + rv, sz);
                    *out = addr; s_ce_isfloat = false; s_ce_isagg = true;
                    return true;
                }
                // Scalar, pointer, or fn-ptr target: all are a single opaque
                // value of Type_Width(elemty) bytes — a comptime pointer is a
                // plain int64_t arena offset, and a comptime fn-value is a
                // Symbol* bit pattern (see AST_IDENT), identical in shape to
                // every other scalar this evaluator handles, so no separate
                // case is needed beyond widening the allowlist past
                // TYPE_PRIMITIVE/TYPE_POINTER to include TYPE_FUNCTION too.
                if (elemty->cls != TYPE_PRIMITIVE && elemty->cls != TYPE_POINTER &&
                    elemty->cls != TYPE_FUNCTION) return false;
                ce_mem_store_typed(addr, rv, elemty); // handles f32 narrowing
                *out = rv; s_ce_isfloat = Type_IsFloat(elemty); s_ce_isagg = false;
                return true;
            }
            if (!node->binary.left || node->binary.left->type != AST_IDENT) return false;
            struct Symbol* tsym = node->binary.left->ident.sym;
            Type* tt = tsym ? tsym->type : NULL;
            // A bare aggregate literal RHS (`p = {.a=1, .b=2}`) may still be
            // fully unresolved here (sdef/elem_type NULL) -- ConstEval's own
            // AST_STRUCT_LITERAL/AST_ARRAY_LITERAL cases refuse to fold an
            // unresolved literal ("not a closed-term value here") rather than
            // resolving it themselves. ce_build_aggregate already does this
            // exact resolve-then-build for a DECLARATION's literal initializer
            // (see its own comment: "const is evaluated at PARSE time, before
            // typecheck runs resolve_brace_literal"); a plain assignment's
            // literal RHS needs the identical treatment, idempotently, since
            // `TYPE name = EXPR` is now sugar over `TYPE name` + `name = EXPR`
            // (parser.c, make_decl_stmt) and no longer resolves the literal at
            // its own dedicated declaration site.
            if (tt && (node->binary.right->type == AST_STRUCT_LITERAL ||
                       node->binary.right->type == AST_ARRAY_LITERAL)) {
                resolve_brace_literal(node->binary.right, tt);
            }
            int64_t v;
            if (!ConstEval(node->binary.right, &v)) return false;
            bool isf = s_ce_isfloat;
            bool rhs_agg = s_ce_isagg;
            // Whole-aggregate assignment (`b = a`, plain ident target — the
            // AST_FIELD/AST_INDEX case above already copies correctly). This
            // must copy INTO b's existing storage, not repoint b's address at
            // a's — ce_set_local's aggregate branch does the latter (a rebind,
            // "b->addr = val"), which was fine for its original purpose
            // (rebinding a name to a fresh comptime-heap object) but wrong
            // here: something could already hold &b, and repointing b's own
            // address out from under that reference would silently break it.
            // Same underlying soundness issue as the declaration-copy fix
            // above, just for the assignment path instead of the decl path.
            if (Type_IsAggregate(tt) && rhs_agg) {
                CEBinding* b = ce_find(tsym);
                if (!b) return false;
                uint64_t sz = Type_SizeOf(tt);
                if ((uint64_t)b->addr + sz > CE_MEM_SIZE || (uint64_t)v + sz > CE_MEM_SIZE) return false;
                memcpy(s_ce_mem + b->addr, s_ce_mem + v, sz);
                *out = (int64_t)b->addr; s_ce_isagg = true; s_ce_isfloat = false;
                return true;
            }
            // Truncate to the target variable's declared width so a comptime
            // `u8 x; x = x + 10` wraps mod 256, matching runtime (was: no wrap).
            if (!isf && tt && tt->cls == TYPE_PRIMITIVE) {
                int w = Type_Width(tt); bool sgn = Type_IsSigned(tt);
                switch (w) {
                    case 1: v = sgn ? (int64_t)(int8_t)v  : (int64_t)(uint8_t)v;  break;
                    case 2: v = sgn ? (int64_t)(int16_t)v : (int64_t)(uint16_t)v; break;
                    case 4: v = sgn ? (int64_t)(int32_t)v : (int64_t)(uint32_t)v; break;
                    default: break;
                }
            }
            if (!ce_set_local(tsym, v, isf)) return false;
            *out = v;
            s_ce_isfloat = isf;
            return true;
}

static bool ce_eval_cast(ASTNode* node, int64_t* out) {
            // A cast in constexpr: fold the operand, then convert to the target.
            // Handles int<->float both ways (e.g. (i32)3.9 -> 3, (f64)5 -> 5.0)
            // and integer truncation ((u8)300 -> 44).
            int64_t v;
            if (!ConstEval(node->cast.expr, &v)) return false;
            bool src_float = s_ce_isfloat;
            Type* t = node->cast.target_type;
            // Cast to a pointer type (e.g. `(u32*)&_m` in match's tag-read): the
            // value is a comptime "address" (arena offset); pass it through
            // unchanged so DEREF can read from it. No width conversion.
            if (t && t->cls == TYPE_POINTER) { *out = v; s_ce_isfloat = false; return true; }
            // Cast to a FUNCTION type: unlike a pointer, there is no legitimate
            // arena-offset representation for an arbitrary folded integer here —
            // a function value can only ever validly come from naming an actual
            // function (AST_IDENT resolving to a SYM_FUNCTION symbol), never from
            // casting an arbitrary int. Passing an arbitrary int through (the way
            // TYPE_POINTER does above) would let e.g. `(fn(u32) u32)(1234)` fold
            // to the literal int 1234, which AST_CALL then treats as a real
            // Symbol* and dereferences — straight into unmapped memory.
            //
            // But the comment above states the exact rule that makes the SAFE case
            // safe: the operand must NAME A REAL FUNCTION. So test that, rather than
            // rejecting every function cast. `(fn(void*))Circle_draw` is a real
            // symbol re-typed -- the value is a genuine code address either way, and
            // the cast only changes how the callee's `self` is spelled. That is
            // precisely the cast an ERASED vtable needs (`fn(Circle*)` -> `fn(void*)`),
            // and rejecting it was what stopped a vtable from living in a const-generic
            // parameter -- i.e. from being ZERO-COST, carried in the type rather than
            // in the value.
            //
            // Anything else (an int literal, an arithmetic expression, a folded
            // address) is still refused, so the unmapped-memory case the original
            // guard existed for stays refused.
            if (t && t->cls == TYPE_FUNCTION) {
                ASTNode* inner = node->cast.expr;
                while (inner && inner->type == AST_CAST) inner = inner->cast.expr;
                // Two shapes name a real function, both equally safe to pass through
                // unchanged (never an arbitrary user integer, which is the case this
                // guard exists to reject):
                //   1. AST_IDENT resolving to a SYM_FUNCTION symbol -- `(fn(void*))Circle_draw`.
                //   2. AST_INT_LITERAL tagged LIT_FN_SYMBOL -- the shape clone_ast builds
                //      for a fn-typed const-generic parameter once it's bound to a concrete
                //      function (types.c: a Symbol* folded into the literal's int_value,
                //      wrapped in exactly this cast to re-type it to the param's declared
                //      fn signature). Without this second case, a zero-cost vtable carried
                //      in a const-generic parameter (see docs/specs.md example: Dyn[F]
                //      where F IS the dispatch function) worked at runtime but could never
                //      be folded a second time inside a `const` -- the runtime path and
                //      the comptime path diverged on the exact same construction.
                bool names_a_function =
                    (inner && inner->type == AST_IDENT && inner->ident.sym &&
                     inner->ident.sym->kind == SYM_FUNCTION) ||
                    (inner && inner->type == AST_INT_LITERAL && inner->lit_kind == LIT_FN_SYMBOL);
                if (!names_a_function) return false;
                *out = v; s_ce_isfloat = false; return true;
            }
            bool dst_float = Type_IsFloat(t);
            if (dst_float) {
                double d = src_float ? ce_bits_to_f(v) : (double)v;
                if (Type_Width(t) == 4) d = (float)d; // f32 narrows
                *out = ce_f_to_bits(d);
                s_ce_isfloat = true;
                return true;
            }
            // target is integer
            if (src_float) v = (int64_t)ce_bits_to_f(v); // float -> int truncation
            int w = Type_Width(t);
            bool sgn = Type_IsSigned(t);
            switch (w) {
                case 1: v = sgn ? (int64_t)(int8_t)v  : (int64_t)(uint8_t)v;  break;
                case 2: v = sgn ? (int64_t)(int16_t)v : (int64_t)(uint16_t)v; break;
                case 4: v = sgn ? (int64_t)(int32_t)v : (int64_t)(uint32_t)v; break;
                default: break; // 8: unchanged
            }
            *out = v;
            s_ce_isfloat = false;
            return true;
}

static bool ce_eval_ident(ASTNode* node, int64_t* out) {
            // A bare function name used as a VALUE (fn-ptr): there's no byte data
            // to fold (a function has no arena presence, unlike a pointer whose
            // value is an arena offset). Mirror the pointer convention with the
            // one representation that already IS stable and unique per function
            // for the life of this compiler run: the Symbol* itself.
            if (node->ident.sym && node->ident.sym->kind == SYM_FUNCTION &&
                !node->ident.sym->generic_decl) {
                *out = (int64_t)(intptr_t)node->ident.sym;
                s_ce_isfloat = false; s_ce_isagg = false; s_ce_isfnsym = true;
                return true;
            }
            // Same, but for a GENERIC function referenced with explicit type args
            // as a value (`id_i32[i32]`, stored in a fn-ptr-typed field/variable,
            // never called directly here) -- instantiate it first to get the real,
            // concrete Symbol*, then fold that exactly like the non-generic case
            // above. Without this, id_i32[i32] silently had no fold path at all:
            // the non-generic branch excludes it (generic_decl is set), and
            // nothing else in this function recognizes an explicit-type-args
            // generic ident either -- so a struct literal field or const
            // initializer storing a generic function reference always failed
            // with "not a constant expression," while the identical shape with
            // a plain (non-generic) function folded fine.
            if (node->ident.sym && node->ident.sym->kind == SYM_FUNCTION &&
                node->ident.sym->generic_decl && node->ident.type_arg_count > 0) {
                Symbol* isym = Generic_Instantiate(node->ident.sym, node->ident.type_args,
                                                    node->ident.type_arg_count);
                *out = (int64_t)(intptr_t)isym;
                s_ce_isfloat = false; s_ce_isagg = false; s_ce_isfnsym = true;
                return true;
            }
            if (s_ce_generic_params) {
                for (size_t i = 0; i < s_ce_generic_n; i++) {
                    if (strlen(s_ce_generic_params[i]) == node->ident.name_len &&
                        strncmp(node->ident.name, s_ce_generic_params[i], node->ident.name_len) == 0) {
                        if (s_ce_generic_args[i]->cls == TYPE_CONST_VALUE) {
                        Type* cv = s_ce_generic_args[i];
                        if (cv->cval.defer) {
                            // Still-deferred value arg (references an outer param not
                            // yet concrete in this frame). Try to fold it here; if that
                            // fails, report unresolved so the enclosing size/expr also
                            // defers rather than silently reading a stale 0.
                            return ConstEval(cv->cval.defer, out);
                        }
                        if (cv->cval.is_agg) {
                            // Aggregate-valued param (e.g. `[Dim D]`): its bytes live
                            // at cval.agg_off in the persistent arena. Return the
                            // offset and mark addressable so field/index access
                            // (D.n) reads it via the normal AST_FIELD/AST_INDEX path.
                            *out = (int64_t)cv->cval.agg_off;
                            s_ce_isagg = true; s_ce_isfloat = false;
                            return true;
                        }
                        *out = cv->cval.scalar;
                        s_ce_isfloat = false;
                        s_ce_isfnsym = (cv->cval.pin && cv->cval.pin->cls == TYPE_FUNCTION);
                        return true;
                    }
                    }
                }
            }
            // First: a comptime param/local bound in the current call/block frame.
            if (node->ident.sym && ce_lookup_local(node->ident.sym, out)) return true;
            // Else: a global named constant (constants may build on earlier constants).
            // A real runtime variable (a sym not in the comptime env) is NOT foldable.
            ConstDef* c = Const_Find(node->ident.name, node->ident.name_len);
            if (!c) return false;
            // A const that's still PENDING (its own initializer forward-referenced a
            // later fn) has only a placeholder value — fail the fold so a const that
            // depends on it (`const B = A * 2`) also defers, instead of capturing 0.
            if (c->pending_expr) return false;
            *out = c->value;
            // LIFTED: a const of aggregate type carries an arena OFFSET as its
            // value — flag it so `&P`, `P.f`, `P[i]` can address into the arena.
            s_ce_isagg = (c->type && (c->type->cls == TYPE_STRUCT ||
                                      c->type->cls == TYPE_ARRAY));
            s_ce_isfloat = false;
            return true;
}

static bool ce_eval_declaration(ASTNode* node, int64_t* out) {
            // A comptime block local: fold its initializer and bind it in the
            // unified store. WALL 4: a nested fn definition isn't an AST_DECLARATION,
            // so it can't sneak in here.
            if (s_ce_env_top >= 256) return false;
            Type* vt = node->decl.sym ? node->decl.sym->type : NULL;
            bool agg = Type_IsAggregate(vt);
            int64_t v;
            bool init_is_addr = false;
            if (!node->decl.init_expr) {
                // Bare declaration, no initializer (`Arr a` / `i32 x`) — legal at
                // runtime (AST_DECLARATION's codegen zero-fills storage before any
                // later field/index writes; see backend_x64.c), so comptime must
                // honor the same guarantee instead of refusing to fold at all.
                // Aggregates get zeroed arena space directly (ce_mem_alloc already
                // memsets — same zero-init invariant the runtime path documents);
                // scalars get a zeroed scalar binding. Either way there's no
                // literal to fill from, so this skips ce_build_aggregate's
                // layout_fill step entirely rather than faking an empty literal.
                if (agg) {
                    v = ce_mem_alloc(Type_SizeOf(vt));
                    if (v < 0) return false;
                    init_is_addr = true;
                } else {
                    v = 0;
                }
            } else if (agg && (node->decl.init_expr->type == AST_STRUCT_LITERAL ||
                        node->decl.init_expr->type == AST_ARRAY_LITERAL)) {
                // Aggregate local built from a literal: materialize into the arena;
                // the local adopts that address (its value IS its address).
                v = ce_build_aggregate(node->decl.init_expr, vt);
                if (v < 0) return false;
                init_is_addr = true;
            } else {
                if (!ConstEval(node->decl.init_expr, &v)) return false;
                // If the initializer produced an aggregate, v is its SOURCE
                // arena address — but `T local = <existing aggregate value>`
                // has value-copy semantics (matching the runtime backend's own
                // AST_DECLARATION, which memcpy's the bytes), not reference
                // semantics. Adopting the source's address directly here was a
                // real, pre-existing soundness bug: `T b = a; b.x = 999` would
                // silently corrupt `a` too, since b and a would be the exact
                // same arena bytes under two names. Copy into fresh storage
                // instead, so the new local is genuinely independent.
                if (agg && s_ce_isagg) {
                    int64_t dst = ce_mem_alloc(Type_SizeOf(vt));
                    if (dst < 0) return false;
                    uint64_t sz = Type_SizeOf(vt);
                    if ((uint64_t)dst + sz > CE_MEM_SIZE || (uint64_t)v + sz > CE_MEM_SIZE) return false;
                    memcpy(s_ce_mem + dst, s_ce_mem + v, sz);
                    v = dst;
                    init_is_addr = true;
                }
            }
            if (!ce_bind_local(node->decl.sym, vt, v, init_is_addr)) return false;
            *out = v;
            s_ce_isagg = agg; s_ce_isfloat = (!agg && Type_IsFloat(vt));
            return true;
}

static bool ce_eval_new(ASTNode* node, int64_t* out) {
            // Comptime heap == the comptime arena. `new T{...}` / `new T[n]`
            // allocates in the arena and returns its offset as a pointer value.
            // The whole point of the unified store: a heap pointer is just an
            // arena address, so new/deref/field/index all work the same as stack
            // aggregates. `delete` (below) is a no-op — the arena reclaims on reset.
            //
            // s_ce_isagg is false in every branch below: Type_Infer's own AST_NEW
            // case says "new T yields T* in both cases" — always a POINTER, never
            // the aggregate T itself, even though the pointer's value happens to
            // be an arena offset (the same representation an aggregate's value
            // uses). Marking it isagg=true (the previous behavior) made a pointer
            // flow through the aggregate-binop comparison path instead of the
            // plain scalar-integer one — comparing pointer VALUES needs a simple
            // == on the addresses, not a byte-for-byte comparison of what they
            // point AT. It also broke null checks: null is bare int 0, so a
            // pointer wrongly flagged isagg would hit "aggregate vs scalar: not
            // comparable" and fail the whole fold instead of comparing cleanly.
            Type* et = node->new_expr.alloc_type;
            if (!et) return false;
            uint64_t esz = Type_SizeOf(et);
            if (node->new_expr.count) {
                // new T[n]: n must fold to a comptime constant.
                int64_t n; if (!ConstEval(node->new_expr.count, &n)) return false;
                if (n < 0) return false;
                int64_t off = ce_mem_alloc(esz * (uint64_t)n);
                if (off < 0) return false;
                *out = off; s_ce_isagg = false; s_ce_isfloat = false;
                return true;
            }
            // new T or new T{...}: one element. Build the initializer into the slot.
            if (node->new_expr.init) {
                resolve_brace_literal(node->new_expr.init, et);
                int64_t off = ce_build_aggregate(node->new_expr.init, et);
                if (off < 0) return false;
                *out = off; s_ce_isagg = false; s_ce_isfloat = false;
                return true;
            }
            int64_t off = ce_mem_alloc(esz ? esz : 1);
            if (off < 0) return false;
            *out = off; s_ce_isagg = false; s_ce_isfloat = false;
            return true;
}

static bool ConstEval_inner(ASTNode* node, int64_t* out) {
    // Operator-overload dispatch: `a op b` / `v[i]` / `!v` / `~v`, where a's
    // (or v's) type defines the matching dunder method, must call THAT, not
    // fall through to the lanewise/scalar/array fold each op's own case below
    // would otherwise do -- exactly like Type_Infer's own dispatch (which
    // ConstEval does NOT go through: ConstEval is a second, independent
    // interpreter that walks the raw AST directly, and Const_ResolvePending
    // runs BEFORE Typecheck_Program, so the lazy AST_ADD -> AST_CALL rewrite
    // Type_Infer does on first visit may never have happened yet for this
    // node when ConstEval gets to it first). MUST run before the switch
    // below, not merely before its `default` -- AST_INDEX (among others) has
    // its own explicit case there and would return from it directly,
    // skipping a check placed only at the switch's fallback. Confirmed by a
    // real bug: placing this check after the switch's `default: break;`
    // fixed __add/__eq (which have no case of their own, so they DO fall to
    // `default`) but silently left __index broken (v[0] still read raw array
    // bytes, wrong element type and all, instead of calling __index) until
    // moved here. Each rewrite mutates `node` in place into an AST_CALL when
    // it applies; re-dispatch on the same node so the existing AST_CALL case
    // below (already how `const R = build()` folds an ordinary function
    // call) does the rest -- no new call-folding logic needed here. (__call
    // is hooked separately, inside ce_eval_call itself, since it needs the
    // call target's already-inferred type first -- see that function's own
    // comment.)
    if (try_rewrite_operator_method(node) || try_rewrite_index_method(node) ||
        try_rewrite_unary_operator_method(node) || try_rewrite_cast_operator(node)) {
        return ConstEval(node, out);
    }

    switch (node->type) {
        case AST_INT_LITERAL:
            // true/false fold to 1/0; null folds to 0 (a zero pointer is a valid constexpr).
            // Explicitly clear both flags here (not just s_ce_isfloat, which every
            // other literal case already does) — s_ce_isagg is otherwise left
            // stale from whatever the previous ConstEval call set it to, which
            // could make a null comparison misclassify as an aggregate-vs-
            // aggregate comparison instead of the plain scalar 0 it actually is.
            if (node->lit_kind == LIT_NULL) { *out = 0; s_ce_isfloat = false; s_ce_isagg = false; return true; }
            if (node->lit_kind == LIT_FLOAT) {
                // A float literal: carry its bit pattern, flag as float. (The old
                // bug read int_value here and silently folded floats to 0.)
                *out = ce_f_to_bits(node->float_value);
                s_ce_isfloat = true;
                return true;
            }
            *out = (int64_t)node->int_value;
            s_ce_isfloat = false;
            return true;

        case AST_STRUCT_LITERAL: {
            // A (resolved) aggregate literal evaluated as a VALUE — e.g.
            // `return Opt[u32].Some{42}` in a comptime fn, or a literal arg.
            // Materialize it into the arena and yield its offset. Reuses the
            // shared layout_fill traversal (the same one the backend uses).
            StructDef* sd = node->struct_lit.sdef;
            if (!sd) return false; // unresolved: not a closed-term value here
            Type st; st.cls = TYPE_STRUCT; st.struct_name = sd->name;
            int64_t off = ce_build_aggregate(node, &st);
            if (off < 0) return false;
            *out = off; s_ce_isagg = true; s_ce_isfloat = false;
            return true;
        }

        case AST_ARRAY_LITERAL: {
            if (!node->array_lit.elem_type) return false;
            Type at; at.cls = TYPE_ARRAY;
            at.array.element = node->array_lit.elem_type;
            at.array.count = node->array_lit.count;
            int64_t off = ce_build_aggregate(node, &at);
            if (off < 0) return false;
            *out = off; s_ce_isagg = true; s_ce_isfloat = false;
            return true;
        }

        case AST_SIZEOF: {
            Type* t = node->sizeof_expr.type;
            if (!t && node->sizeof_expr.defer_expr) {
                // Parse-time couldn't determine the operand's type (it referenced
                // a generic param in a shape more complex than a bare value-param
                // identifier). Now that we may be inside an active substitution
                // frame, re-run Type_Infer on the stashed operand — AST_IDENT
                // nodes for bound params resolve their value via this same frame
                // elsewhere in ConstEval, but here we need the operand's TYPE, so
                // this goes through Type_Infer, not ConstEval, on the raw operand.
                t = Type_Infer(node->sizeof_expr.defer_expr);
                if (!t) return false; // still unresolved — defer further up
            }
            if (s_ce_generic_params) {
                t = Type_Substitute(t, s_ce_generic_params, s_ce_generic_args, s_ce_generic_n);
            }
            *out = (int64_t)Type_SizeOf(t);
            s_ce_isfloat = false;
            return true;
        }

        case AST_ALIGNOF: {
            // Same shape as AST_SIZEOF right above, just Type_AlignOf instead
            // of Type_SizeOf; alignof has no expr-operand form so there's no
            // defer_expr branch to mirror.
            Type* t = node->sizeof_expr.type;
            if (s_ce_generic_params) {
                t = Type_Substitute(t, s_ce_generic_params, s_ce_generic_args, s_ce_generic_n);
            }
            *out = (int64_t)Type_AlignOf(t);
            s_ce_isfloat = false;
            return true;
        }

        case AST_OFFSETOF: {
            // Mirrors AST_SIZEOF's substitution dance, then looks up the
            // i-th field's offset by index (not name) — same convention the
            // language spec fixes for offsetof/nameof.
            Type* t = node->field_ref_expr.type;
            if (s_ce_generic_params) {
                t = Type_Substitute(t, s_ce_generic_params, s_ce_generic_args, s_ce_generic_n);
            }
            if (!t || t->cls != TYPE_STRUCT) return false;
            int64_t idx;
            if (!ConstEval(node->field_ref_expr.index_expr, &idx)) return false;
            StructDef* sd = Struct_Find(t->struct_name);
            if (!sd) return false;
            Struct_Layout(sd);
            if (idx < 0 || (uint64_t)idx >= sd->field_count) {
                Error_AtNode(node, "offsetof: field index out of range", NULL);
            }
            *out = (int64_t)sd->fields[idx].offset;
            s_ce_isfloat = false;
            return true;
        }

        case AST_TYPE_EXPR: {
            // A bare generic-param reference used as a type-position value
            // (T == i32, or N inside T[N] once N is uniformly just another
            // TYPE_PARAM per s_param_kinds's own convention — a NULL kind
            // entry, not a separate category of parameter). Fold exactly
            // the way AST_IDENT already folds a generic param's binding:
            // if the current substitution frame bound this name to a
            // TYPE_CONST_VALUE (e.g. N inferred as an array size from a
            // call-site argument), unwrap its scalar — same representation
            // every other const-generic value already uses throughout this
            // evaluator. If it's bound to an ordinary Type* instead (a real
            // type, e.g. T bound to i32), there's no number to produce:
            // return false and let the type-comparison case resolve
            // elsewhere (typecheck/codegen), not here.
            Type* pt = node->sizeof_expr.type;
            if (!pt || pt->cls != TYPE_PARAM || !pt->param_name || !s_ce_generic_params) return false;
            for (size_t i = 0; i < s_ce_generic_n; i++) {
                if (strcmp(s_ce_generic_params[i], pt->param_name) == 0) {
                    Type* cv = s_ce_generic_args[i];
                    if (!cv || cv->cls != TYPE_CONST_VALUE) return false; // a real type, not a value
                    if (cv->cval.defer) return ConstEval(cv->cval.defer, out);
                    if (cv->cval.is_agg) {
                        *out = (int64_t)cv->cval.agg_off;
                        s_ce_isagg = true; s_ce_isfloat = false;
                        return true;
                    }
                    *out = cv->cval.scalar;
                    s_ce_isfloat = false;
                    return true;
                }
            }
            return false;
        }

        case AST_IDENT:
            return ce_eval_ident(node, out);

        case AST_CALL:
            return ce_eval_call(node, out);

        case AST_RETURN: {
            // A function body's value is its return expression — EXCEPT when the
            // enclosing function is void (return_type NULL or explicit `void`),
            // where a bare `return` is ordinary early-exit control flow, not "this
            // isn't a foldable constant." Previously any bare `return` bailed the
            // whole fold unconditionally, which meant NO void-returning function
            // could ever be called from a const initializer — killing any
            // `touch(&t)` / `inorder(tree, &out)`-shaped helper that mutates
            // through a pointer purely for its side effects, exactly the pattern
            // a struct-building comptime function naturally wants.
            bool is_void = !s_ce_ret_type ||
                           (s_ce_ret_type->cls == TYPE_PRIMITIVE &&
                            s_ce_ret_type->primitive == PRIM_VOID);
            if (!node->unary) {
                if (!is_void) return false; // a value-producing fn needs a value
                *out = 0; s_ce_isagg = false; s_ce_isfloat = false;
                s_ce_returned = true;
                return true;
            }
            // A bare `{...}`/`.Variant` in return position gets its type from the
            // enclosing fn's return type (resolved during typecheck for runtime, but
            // const runs at parse time — resolve it here against the call's ret type).
            if (s_ce_ret_type &&
                (node->unary->type == AST_STRUCT_LITERAL || node->unary->type == AST_ARRAY_LITERAL))
                resolve_brace_literal(node->unary, s_ce_ret_type);
            if (!ConstEval(node->unary, out)) return false;
            s_ce_returned = true;
            return true;
        }

        case AST_DECLARATION:
            return ce_eval_declaration(node, out);

        case AST_CAST:
            return ce_eval_cast(node, out);

        case AST_FIELD: {
            // Read a field from a comptime aggregate: fold the base to an arena
            // offset, then load the field's bytes at base + field->offset. (Reuses
            // the SAME layout the backend/sink use — StructField.offset / widths.)
            int64_t base; if (!ConstEval(node->field.base, &base)) return false;
            if (!ce_base_is_addressable(node->field.base)) return false;
            Type* bt = ce_layout_type(Type_Infer(node->field.base));
            StructDef* sd = bt ? Struct_Find(bt->struct_name) : NULL;
            if (!sd) return false;
            StructField* f = Struct_FindField(sd, node->field.field_name, node->field.field_name_len);
            if (!f) return false;
            if (Type_IsAggregate(f->type)) {
                // nested aggregate field: its value is the arena offset of the field
                *out = base + (int64_t)f->offset;
                s_ce_isagg = true; s_ce_isfloat = false;
                return true;
            }
            *out = ce_mem_load_typed((uint32_t)(base + f->offset), f->type); // widens f32->f64 bits
            s_ce_isagg = false; s_ce_isfloat = Type_IsFloat(f->type);
            return true;
        }

        case AST_INDEX: {
            // Read an element from a comptime array: base offset + i*elem_size.
            int64_t base; if (!ConstEval(node->index.base, &base)) return false;
            if (!ce_base_is_addressable(node->index.base)) return false;
            Type* bt = ce_layout_type(Type_Infer(node->index.base));
            Type* et = bt ? (bt->cls == TYPE_ARRAY ? bt->array.element : bt) : NULL;
            if (!et) return false;
            int64_t idx;
            if (!ConstEval(node->index.index, &idx)) return false;
            uint64_t esz = Type_SizeOf(et);
            uint32_t eoff = (uint32_t)(base + idx * (int64_t)esz);
            if (Type_IsAggregate(et)) {
                *out = (int64_t)eoff; s_ce_isagg = true; s_ce_isfloat = false; return true;
            }
            *out = ce_mem_load_typed(eoff, et); // widens f32->f64 bits
            s_ce_isagg = false; s_ce_isfloat = Type_IsFloat(et);
            return true;
        }

        case AST_ADDR: {
            // &lvalue: the address is the lvalue's ARENA OFFSET. Handled generically
            // for any lvalue (ident-aggregate, field, index) via ce_lvalue_addr —
            // no special-casing. `match` works because it lowers to `*(u32*)&_m`,
            // and now `&s.field` / `&arr[i]` work too, for free. The offset is an
            // internal address: deref reads back into the arena; it can't escape
            // (a const can't BE a pointer — the result boundary blocks it).
            uint32_t addr; Type* ty;
            if (!ce_lvalue_addr(node->unary, &addr, &ty)) return false;
            *out = (int64_t)addr;
            // An arena address is structurally the aggregate it points at: keep isagg
            // when the pointee is agg so .f/[i] fold through the direct-aggregate path.
            s_ce_isagg = Type_IsAggregate(ty);
            s_ce_isfloat = false;
            return true;
        }

        case AST_DEREF: {
            // *p where p is a comptime "address" (an arena offset from &aggregate
            // or new). off IS the pointee's location either way — the two cases
            // differ only in what *out should become from there.
            int64_t off; if (!ConstEval(node->unary, &off)) return false;
            Type* pt = Type_Infer(node->unary);
            Type* pointee = (pt && pt->cls == TYPE_POINTER) ? pt->pointer_base : NULL;
            if (!pointee) return false;
            if (Type_IsAggregate(pointee)) {
                // Aggregate pointee: previously rejected outright (scalar-only
                // guard), which meant `*p` for a struct/array/enum pointer could
                // never fold — including as a match scrutinee, since match always
                // lowers to `EnumType _m = <scrutinee>`, and any comptime `T local
                // = *p` for aggregate T hit this same wall. An aggregate's value
                // IS its address (same convention as AST_IDENT, AST_NEW,
                // ce_lvalue_addr elsewhere in this file) — no load needed, the
                // pointee's own address is already the correct value to yield.
                *out = off; s_ce_isagg = true; s_ce_isfloat = false;
                return true;
            }
            // Scalar pointee: reads the pointed-to value from the arena. The
            // pointee width comes from the pointer's base type (e.g.
            // `*(u32*)addr` -> 4 bytes). A pointer-to-function pointee is the
            // same opaque 8-byte Symbol* bit pattern as any other fn-value
            // (see AST_IDENT) — widen past TYPE_PRIMITIVE to include it.
            if (pointee->cls != TYPE_PRIMITIVE && pointee->cls != TYPE_FUNCTION) return false;
            *out = ce_mem_load((uint32_t)off, Type_Width(pointee), Type_IsSigned(pointee));
            s_ce_isagg = false; s_ce_isfloat = Type_IsFloat(pointee);
            return true;
        }

        case AST_NEW:
            return ce_eval_new(node, out);

        case AST_DELETE:
            // Comptime free is a no-op: the arena is bump-allocated and reclaimed
            // wholesale on each eval reset. (Deliberate: comptime allocation is
            // bounded and short-lived, so there's no fragmentation to manage.)
            *out = 0; s_ce_isagg = false; s_ce_isfloat = false;
            return true;

        case AST_LOGICAL_NOT: {
            int64_t v;
            if (!ConstEval(node->unary, &v)) return false;
            *out = !v;
            s_ce_isfloat = false;
            return true;
        }

        case AST_BIT_NOT: {
            int64_t v;
            if (!ConstEval(node->unary, &v)) return false;
            *out = ~v;
            s_ce_isfloat = false;
            return true;
        }

        case AST_IF: {
            // `match T { ... }` used to reach here unresolved (a reflect_pattern
            // AST_IF whose "condition" isn't a real boolean expression at all),
            // because ConstEval walks generic-instantiated bodies that skip
            // Typecheck_Tree entirely. That's now resolved once, eagerly, in
            // Generic_Instantiate (see Resolve_Reflect_Matches in types.c),
            // immediately after clone_ast — so every AST_IF ConstEval ever sees
            // is guaranteed to be an ordinary boolean if, exactly like every
            // other node this function handles. No type-reflection awareness
            // belongs here, and none is needed anymore.
            int64_t cond;
            if (!ConstEval(node->if_stmt.condition, &cond)) return false;
            if (cond) {
                return ConstEval(node->if_stmt.true_block, out);
            } else if (node->if_stmt.false_block) {
                return ConstEval(node->if_stmt.false_block, out);
            } else {
                *out = 0;
                return true;
            }
        }

        case AST_BLOCK: {
            // Locals declared inside this block live only for its duration; record the
            // env high-water mark and unwind to it on exit (B1: block-scoped comptime
            // locals). A `return` inside stops the block and yields its value (so a
            // function body `{ ...; return X; ... }` evaluates to X, not the last stmt).
            //
            // EXCEPTION: a `transparent` block (currently only unpack's synthetic
            // wrapper) isn't a real scope -- it exists purely because parse_statement
            // has to return one node. Its bindings must survive into the enclosing
            // scope, exactly like the symbol table already treats them at parse time
            // (see parse_unpack). Skipping the unwind here is what makes that promise
            // hold during const-folding too, not just at runtime.
            int frame_base = s_ce_env_top;
            *out = 0;
            bool ok = true;
            for (size_t i = 0; i < node->block.count; i++) {
                if (!ConstEval(node->block.statements[i], out)) { ok = false; break; }
                if (s_ce_returned || s_ce_broke || s_ce_continued) break; // stop the block
            }
            if (!node->block.transparent) s_ce_env_top = frame_base; // unwind block locals
            return ok;
        }

        case AST_BREAK:    s_ce_broke = true;     *out = 0; return true;
        case AST_CONTINUE: s_ce_continued = true; *out = 0; return true;

        case AST_ASSIGN:
            return ce_eval_assign(node, out);

        case AST_WHILE: {
            // Bounded compile-time iteration. The shared step budget (s_ce_budget)
            // guards against infinite loops — an over-long comptime loop simply fails
            // to fold (returns false), it does not hang the compiler.
            *out = 0;
            while (1) {
                if (s_ce_budget > 0 && --s_ce_budget <= 0) return false;
                int64_t cond;
                if (!ConstEval(node->while_stmt.condition, &cond)) return false;
                if (!cond) break;
                int64_t dummy;
                if (!ConstEval(node->while_stmt.body, &dummy)) return false;
                if (s_ce_returned) { *out = dummy; break; }
                if (s_ce_broke) { s_ce_broke = false; break; }   // break exits the loop
                s_ce_continued = false;                          // continue: next iteration
            }
            return true;
        }

        case AST_FOR: {
            // Desugared as init; while(cond) { body; incr }. init binds the loop var as
            // a comptime local in the current frame; unwind it on exit.
            int frame_base = s_ce_env_top;
            int64_t tmp;
            if (node->for_stmt.init && !ConstEval(node->for_stmt.init, &tmp)) { s_ce_env_top = frame_base; return false; }
            *out = 0;
            while (1) {
                if (s_ce_budget > 0 && --s_ce_budget <= 0) { s_ce_env_top = frame_base; return false; }
                if (node->for_stmt.cond) {
                    int64_t cond;
                    if (!ConstEval(node->for_stmt.cond, &cond)) { s_ce_env_top = frame_base; return false; }
                    if (!cond) break;
                }
                if (node->for_stmt.body && !ConstEval(node->for_stmt.body, &tmp)) { s_ce_env_top = frame_base; return false; }
                if (s_ce_returned) { *out = tmp; break; }   // early return: propagate the value (was lost -> 0)
                if (s_ce_broke) { s_ce_broke = false; break; }   // break exits
                s_ce_continued = false;                          // continue still runs incr
                if (node->for_stmt.incr && !ConstEval(node->for_stmt.incr, &tmp)) { s_ce_env_top = frame_base; return false; }
            }
            s_ce_env_top = frame_base; // unwind the loop variable
            return true;
        }

        default: break;
    }

    // Binary operators: both sides must fold. Each side reports int-or-float via
    // s_ce_isfloat; if EITHER side is float, the op is a float op (the int side is
    // promoted), mirroring runtime numeric coercion.
    int64_t a, b;
    bool af, bf;
    switch (node->type) {
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
        case AST_SHL: case AST_SHR:
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR: {
            // Bare type-comparison-as-expression (T == i32) is REMOVED (see
            // types.c's Typecheck_Tree default case for the full reasoning).
            // No special fold needed here: ConstEval on an AST_TYPE_EXPR that
            // wraps a real (non-const-value) type already returns false on
            // its own (this function's own AST_TYPE_EXPR case, a few hundred
            // lines up), which correctly surfaces as "not a constant
            // expression" -- the right failure for a removed feature.
            bool ag_a, ag_b;
            if (!ConstEval(node->binary.left, &a)) return false;
            af = s_ce_isfloat; ag_a = s_ce_isagg;
            if (!ConstEval(node->binary.right, &b)) return false;
            bf = s_ce_isfloat; ag_b = s_ce_isagg;

            // Aggregate operand: this op reached the binop dispatch with a struct/
            // array value on (at least) one side. Those values are arena OFFSETS,
            // not comparable/addable scalars -- doing `a == b` or `a + b` here
            // would operate on WHERE the aggregates are stored, not WHAT they
            // contain, and silently produce a wrong answer. Route through the
            // shared agg_binop_apply (codegen.c) -- the same op-legality check and
            // lane enumeration the x86 backend uses -- so constexpr can't drift
            // from runtime about which ops are aggregate-legal.
            if (ag_a || ag_b) {
                if (!ag_a || !ag_b) return false; // aggregate vs scalar: not comparable
                Type* t = Type_Infer(node->binary.left);
                if (!t) t = Type_Infer(node->binary.right);
                if (!t) return false;
                bool result_is_agg;
                if (!ce_agg_binop(node->type, t, (uint32_t)a, (uint32_t)b, out, &result_is_agg))
                    return false;
                s_ce_isfloat = false; s_ce_isagg = result_is_agg;
                return true;
            }
            break;
        }
        default:
            return false;
    }

    // Float arithmetic path: promote the int operand, compute in double, bitcast
    // the result back. Comparisons yield an int bool; arithmetic yields a float.
    if (af || bf) {
        double x = af ? ce_bits_to_f(a) : (double)a;
        double y = bf ? ce_bits_to_f(b) : (double)b;
        switch (node->type) {
            case AST_ADD: *out = ce_f_to_bits(x + y); s_ce_isfloat = true;  return true;
            case AST_SUB: *out = ce_f_to_bits(x - y); s_ce_isfloat = true;  return true;
            case AST_MUL: *out = ce_f_to_bits(x * y); s_ce_isfloat = true;  return true;
            case AST_DIV: if (y == 0.0) return false; *out = ce_f_to_bits(x / y); s_ce_isfloat = true; return true;
            case AST_EQ:  *out = (x == y); s_ce_isfloat = false; return true;
            case AST_NEQ: *out = (x != y); s_ce_isfloat = false; return true;
            case AST_LT:  *out = (x <  y); s_ce_isfloat = false; return true;
            case AST_GT:  *out = (x >  y); s_ce_isfloat = false; return true;
            case AST_LTE: *out = (x <= y); s_ce_isfloat = false; return true;
            case AST_GTE: *out = (x >= y); s_ce_isfloat = false; return true;
            default: return false; // %, bitwise, logical not defined on floats
        }
    }

    // Integer path.
    s_ce_isfloat = false;
    switch (node->type) {
        case AST_ADD: *out = a + b; return true;
        case AST_SUB: *out = a - b; return true;
        case AST_MUL: *out = a * b; return true;
        case AST_DIV: if (b == 0) return false; *out = a / b; return true;
        case AST_MOD: if (b == 0) return false; *out = a % b; return true;
        case AST_BIT_AND: *out = a & b; return true;
        case AST_BIT_OR:  *out = a | b; return true;
        case AST_BIT_XOR: *out = a ^ b; return true;
        case AST_SHL: *out = a << b; return true;
        case AST_SHR: *out = a >> b; return true;
        case AST_EQ:  *out = (a == b); return true;
        case AST_NEQ: *out = (a != b); return true;
        case AST_LT:  *out = (a <  b); return true;
        case AST_GT:  *out = (a >  b); return true;
        case AST_LTE: *out = (a <= b); return true;
        case AST_GTE: *out = (a >= b); return true;
        case AST_LOGICAL_AND: *out = (a && b); return true;
        case AST_LOGICAL_OR:  *out = (a || b); return true;
        default: return false;
    }
}