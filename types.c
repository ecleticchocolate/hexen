#include "compiler.h"
#include "codegen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// A TYPE_FN_LITERAL carries the same calling-convention shape as its underlying
// TYPE_FUNCTION (fn_lit.sig) plus one extra bit of information -- WHICH function.
// Every consumer that only cares about the shape (assignability to a `fn(...) T`
// variable, argument/return checks, codegen's param/return handling) should see
// straight through to that shape and behave exactly as it does for a plain
// TYPE_FUNCTION today. Only unification and struct/type identity (Type_Equals,
// unify_types, type_keystr, Type_ToString) need to see the literal itself,
// because identity is the entire point of the type. This helper is the single
// unwrap point: pass any Type* through it before a shape-only TYPE_FUNCTION
// check, and it's a no-op for every other type class. Exported (not static) so
// the backend can use it too at the call-codegen direct-vs-indirect decision.
Type* Type_FnLitShape(Type* t) {
    return (t && t->cls == TYPE_FN_LITERAL) ? t->fn_lit.sig : t;
}
#define fn_lit_shape Type_FnLitShape

// --- Primitive classification ---

// The one shared width table for every REAL (non-void) primitive. Exported
// (not static) so Type_SizeOf (structs.c) can build on the exact same table
// instead of hand-retyping it -- which it used to do, and which had silently
// drifted: its copy returned 0 for PRIM_V/PRIM_VOID where this one returns 8.
// PRIM_V/PRIM_VOID is deliberately NOT handled here -- see Type_Width and
// Type_SizeOf below, which each answer that case differently on purpose
// (register-context fallback vs. storage size), not by accident.
int Prim_Width(PrimitiveKind p) {
    switch (p) {
        case PRIM_U8:  case PRIM_I8:  case PRIM_BOOL: return 1;
        case PRIM_U16: case PRIM_I16:                return 2;
        case PRIM_U32: case PRIM_I32: case PRIM_F32: return 4;
        case PRIM_U64: case PRIM_I64: case PRIM_F64: return 8;
        case PRIM_V:   case PRIM_VOID:               return -1; // caller must special-case
    }
    return -1;
}

static bool prim_signed(PrimitiveKind p) {
    switch (p) {
        case PRIM_I8: case PRIM_I16: case PRIM_I32: case PRIM_I64: return true;
        default: return false; // unsigned ints, bool, floats(handled separately), v
    }
}

// Structural equality. Never compare Type* by pointer identity: Type_Infer and
// the make_* helpers mint fresh Type objects, so two structurally-identical types
// (e.g. two `u32*`) are distinct pointers. This recurses so nested pointers and
// arrays compare correctly. Extend here when structs/arrays gain real members.
bool Type_Equals(const Type* a, const Type* b);

// Is this "no value" -- an OMITTED type (parser leaves NULL: a function's return
// with nothing written, an enum variant with no payload) or an EXPLICIT `void`
// (a real PRIM_VOID/PRIM_V type node)? Two spellings of the identical thing;
// shared by fn_ret_equal below and reflect_unify (reflections.c), where a
// no-payload enum variant's wildcard-bound H must compare equal to a literal
// `void` pattern the same way an omitted return type already compares equal to
// an explicit one.
bool Type_IsVoidLike(const Type* t) {
    return !t || (t->cls == TYPE_PRIMITIVE &&
                  (t->primitive == PRIM_VOID || t->primitive == PRIM_V));
}

// Is this return type "no return value"? Two spellings reach here: an OMITTED return
// (`fn(u32)`), which the parser leaves as NULL, and an EXPLICIT `void` (`fn(u32) void`),
// which is a real PRIM_VOID/PRIM_V type node. Same meaning, so treat them alike.
static bool ret_is_void(const Type* t) { return Type_IsVoidLike(t); }

// Return-type equality for function types, normalizing the two void spellings.
static bool fn_ret_equal(const Type* a, const Type* b) {
    if (ret_is_void(a) && ret_is_void(b)) return true;
    return Type_Equals(a, b);
}

bool Type_Equals(const Type* a, const Type* b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->cls != b->cls) return false;
    switch (a->cls) {
        case TYPE_PRIMITIVE: return a->primitive == b->primitive;
        case TYPE_POINTER:   return Type_Equals(a->pointer_base, b->pointer_base);
        case TYPE_ARRAY:     return a->array.count == b->array.count && Type_Equals(a->array.element, b->array.element);
        case TYPE_STRUCT:    return (strcmp(a->struct_name, b->struct_name) == 0);
        case TYPE_FUNCTION:
            // A function's return type has TWO spellings of the same thing: omitted
            // (`fn(void*)`, carried as NULL) and explicit (`fn(void*) void`, carried as
            // PRIM_VOID). They denote the identical type, so they must COMPARE equal --
            // otherwise `fn(void*) g = f` fails against an `f` declared with `void`, and
            // a vtable field spelled one way can't hold a function spelled the other.
            // Normalize here rather than at every construction site: this is the one
            // place that decides what "the same function type" means.
            if (!fn_ret_equal(a->function.return_type, b->function.return_type)) return false;
            if (a->function.param_count != b->function.param_count) return false;
            for (size_t i = 0; i < a->function.param_count; i++) {
                if (!Type_Equals(a->function.param_types[i], b->function.param_types[i])) return false;
            }
            return true;
        case TYPE_PARAM:
            return (strcmp(a->param_name, b->param_name) == 0);
        case TYPE_FN_LITERAL:
            // Nominal, not structural: two literals are the same type iff they
            // name the SAME function symbol. This is the entire point of the
            // type -- `asc` and `desc` share a signature but must never compare
            // equal here, or Box[fn_of(asc)] and Box[fn_of(desc)] would collapse
            // back into one StructDef the way plain TYPE_FUNCTION does today.
            return a->fn_lit.sym == b->fn_lit.sym;
        case TYPE_CONST_VALUE: {
            bool ad = a->cval.defer, bd = b->cval.defer;
            if (ad != bd) return false;
            if (ad) {
                // both deferred: equal iff same symbolic ident
                ASTNode *x = a->cval.defer, *y = b->cval.defer;
                if (x->type == AST_IDENT && y->type == AST_IDENT)
                    return x->ident.name_len == y->ident.name_len &&
                           strncmp(x->ident.name, y->ident.name, x->ident.name_len) == 0;
                return x == y; // compound: identity
            }
            if (a->cval.is_agg || b->cval.is_agg) {
                if (a->cval.is_agg != b->cval.is_agg) return false;
                uint64_t sza = a->cval.pin ? Type_SizeOf(a->cval.pin) : 0;
                uint64_t szb = b->cval.pin ? Type_SizeOf(b->cval.pin) : 0;
                if (sza != szb) return false;
                if (sza > 256) sza = 256;
                uint8_t ba[256], bb[256];
                if (!ConstEval_ReadBytes(a->cval.agg_off, ba, sza)) return false;
                if (!ConstEval_ReadBytes(b->cval.agg_off, bb, sza)) return false;
                return memcmp(ba, bb, sza) == 0;
            }
            return a->cval.scalar == b->cval.scalar;
        }
    }
    return false;
}

uint64_t Type_AlignOf(const Type* t) {
    if (!t) return 8;
    switch (t->cls) {
        case TYPE_PRIMITIVE:
            // void/v has 0 bytes of STORAGE (Type_SizeOf), so it imposes no alignment
            // requirement -- must NOT share Type_Width's register-context fallback (8),
            // which exists only for register moves and was never meant to be read as
            // an alignment (see Type_Width's own comment).
            if (t->primitive == PRIM_VOID || t->primitive == PRIM_V) return 1;
            return (uint64_t)Type_Width(t);
        case TYPE_POINTER:   return 8;
        case TYPE_ARRAY:     return Type_AlignOf(t->array.element);
        case TYPE_STRUCT: {
            StructDef* sd = Struct_Find(t->struct_name);
            if (!sd) return 1;
            Struct_Layout(sd);
            return sd->align;
        }
        case TYPE_FUNCTION:  return 8;
        case TYPE_FN_LITERAL: return 8;
        case TYPE_PARAM:     return 8;
    }
    return 8;
}

void Type_ToString(const Type* t, char* out, size_t cap) {
    if (!t) {
        snprintf(out, cap, "void");
        return;
    }
    switch (t->cls) {
        // A pattern, not an inhabited type -- but it still has to PRINT, because a
        // `match` arm that fails to unify wants to name the pattern it tried. Renders
        // as the source spelling: `impl { fn free()  fn len() u32 }`.
        case TYPE_IMPL: {
            size_t off = snprintf(out, cap, "impl {");
            for (size_t m = 0; m < t->impl_pat.method_count && off < cap; m++) {
                char sig[256]; sig[0] = '\0';
                size_t so = 0;
                Type* s = t->impl_pat.sigs[m];
                for (size_t i = 0; s && i < s->function.param_count && so < sizeof(sig); i++) {
                    char pt[128];
                    Type_ToString(s->function.param_types[i], pt, sizeof(pt));
                    so += snprintf(sig + so, sizeof(sig) - so, "%s%s", i ? ", " : "", pt);
                }
                char rt[128]; rt[0] = '\0';
                if (s && s->function.return_type)
                    Type_ToString(s->function.return_type, rt, sizeof(rt));
                off += snprintf(out + off, cap - off, " fn %.*s(%s)%s%s",
                                (int)t->impl_pat.method_name_lens[m],
                                t->impl_pat.method_names[m],
                                sig, rt[0] ? " " : "", rt);
            }
            if (off < cap) snprintf(out + off, cap - off, " }");
            break;
        }
        case TYPE_PRIMITIVE: {
            const char* name = "unknown";
            switch (t->primitive) {
                case PRIM_U8: name = "u8"; break;
                case PRIM_U16: name = "u16"; break;
                case PRIM_U32: name = "u32"; break;
                case PRIM_U64: name = "u64"; break;
                case PRIM_I8: name = "i8"; break;
                case PRIM_I16: name = "i16"; break;
                case PRIM_I32: name = "i32"; break;
                case PRIM_I64: name = "i64"; break;
                case PRIM_BOOL: name = "bool"; break;
                case PRIM_F32: name = "f32"; break;
                case PRIM_F64: name = "f64"; break;
                case PRIM_VOID: name = "void"; break;
                case PRIM_V: name = "void"; break; // the pointee marker for void*; renders as valid Torrent
            }
            snprintf(out, cap, "%s", name);
            break;
        }
        case TYPE_POINTER: {
            char base[256];
            Type_ToString(t->pointer_base, base, sizeof(base));
            if (t->pointer_base && t->pointer_base->cls == TYPE_FUNCTION) {
                snprintf(out, cap, "(%s)*", base);
            } else {
                snprintf(out, cap, "%s*", base);
            }
            break;
        }
        case TYPE_ARRAY: {
            Type* curr = t;
            uint64_t counts[16];
            const char* params[16] = {0};
            int ndims = 0;
            while (curr && curr->cls == TYPE_ARRAY && ndims < 16) {
                counts[ndims] = curr->array.count;
                params[ndims] = curr->array.size_param;
                curr = curr->array.element;
                ndims++;
            }
            char elem[256];
            Type_ToString(curr, elem, sizeof(elem));
            
            int written = 0;
            if (curr && curr->cls == TYPE_FUNCTION) {
                written = snprintf(out, cap, "(%s)", elem);
            } else {
                written = snprintf(out, cap, "%s", elem);
            }
            
            for (int i = 0; i < ndims; i++) {
                if (params[i]) {
                    written += snprintf(out + written, cap - written, "[%s]", params[i]);
                } else {
                    written += snprintf(out + written, cap - written, "[%llu]", (unsigned long long)counts[i]);
                }
            }
            break;
        }
        case TYPE_STRUCT: {
            snprintf(out, cap, "%s", t->struct_name ? t->struct_name : "unknown_struct");
            break;
        }
        case TYPE_PARAM: {
            snprintf(out, cap, "%s", t->param_name ? t->param_name : "T");
            break;
        }
        case TYPE_CONST_VALUE: {
            if (t->cval.defer) {
                // Deferred value (references an in-scope param, e.g. the `N` in a
                // generic return type `W[T, N]`): mangle symbolically so the
                // instance name stays a TEMPLATE (W$T_N) and re-substitutes at
                // monomorphization, instead of baking a stale 0 (W$T_0).
                ASTNode* d = t->cval.defer;
                if (d->type == AST_IDENT) {
                    snprintf(out, cap, "%.*s", (int)d->ident.name_len, d->ident.name);
                } else {
                    snprintf(out, cap, "expr"); // compound deferred expr: opaque tag
                }
            } else if (t->cval.is_agg && t->cval.pin) {
                // Aggregate value: mangle by a short hash of its bytes so distinct
                // aggregates instantiate as distinct types (not a colliding "0").
                uint64_t sz = Type_SizeOf(t->cval.pin);
                if (sz > 64) sz = 64;
                uint8_t buf[64];
                uint64_t h = 1469598103934665603ULL; // FNV-1a
                if (ConstEval_ReadBytes(t->cval.agg_off, buf, sz)) {
                    for (uint64_t k = 0; k < sz; k++) { h ^= buf[k]; h *= 1099511628211ULL; }
                }
                snprintf(out, cap, "a%llx", (unsigned long long)h);
            } else {
                snprintf(out, cap, "%lld", (long long)t->cval.scalar);
            }
            break;
        }
        case TYPE_FUNCTION: {
            // e.g. fn(u32, f32) i32
            int written = snprintf(out, cap, "fn(");
            for (size_t i = 0; i < t->function.param_count; i++) {
                char pbuf[256];
                Type_ToString(t->function.param_types[i], pbuf, sizeof(pbuf));
                written += snprintf(out + written, cap - written, "%s%s", pbuf, 
                                    (i + 1 < t->function.param_count) ? ", " : "");
            }
            if (t->function.is_vararg) {
                written += snprintf(out + written, cap - written, "%s...", 
                                    t->function.param_count > 0 ? ", " : "");
            }
            written += snprintf(out + written, cap - written, ")");
            
            if (t->function.return_type) {
                char rbuf[256];
                Type_ToString(t->function.return_type, rbuf, sizeof(rbuf));
                snprintf(out + written, cap - written, " %s", rbuf);
            }
            break;
        }
        case TYPE_FN_LITERAL: {
            // Render as "fn_of(<name>)" -- distinct from plain "fn(...)" so it
            // can never collide with a structural function-pointer type's
            // rendering, and distinct PER SYMBOL so it feeds Struct_Register's
            // content-derived generic-instance naming (mirrors how anon structs
            // dedup by rendered field types): Box[Cmp] instantiated with `asc`
            // renders as "Box$fn_of(asc)", with `desc` as "Box$fn_of(desc)" --
            // two different StructDefs, which is the whole point.
            snprintf(out, cap, "fn_of(%.*s)",
                     t->fn_lit.sym && t->fn_lit.sym->name ? (int)t->fn_lit.sym->name_len : 7,
                     t->fn_lit.sym && t->fn_lit.sym->name ? t->fn_lit.sym->name : "unknown");
            break;
        }
    }
}


bool Type_IsSigned(const Type* t) {
    if (!t) return false;
    if (t->cls == TYPE_PRIMITIVE) return prim_signed(t->primitive);
    return false; // pointers, arrays, structs are not signed integers
}

bool Type_IsFloat(const Type* t) {
    return t && t->cls == TYPE_PRIMITIVE &&
           (t->primitive == PRIM_F32 || t->primitive == PRIM_F64);
}

// Struct or array: an aggregate value's storage IS its identity in this backend
// (a variable/temp/field of this type is always addressed, never held directly
// in a register the way a scalar is) -- every one of parser.c/backend_x64.c/
// constexpr.c/codegen.c needs this same classification to decide "copy by
// address" vs "load/store by value". Single source of truth: this used to be
// hand-retyped at 21 call sites (plus a separate, constexpr.c-local copy,
// ce_type_is_agg) instead of shared from here.
bool Type_IsAggregate(const Type* t) {
    return t && (t->cls == TYPE_STRUCT || t->cls == TYPE_ARRAY);
}

int Type_Width(const Type* t) {
    if (!t) return 8;
    switch (t->cls) {
        case TYPE_PRIMITIVE: {
            int w = Prim_Width(t->primitive);
            // void/v: register-context fallback -- there's no value to load/store for
            // a void-typed expression, so this width is never actually read off a real
            // register move; 8 just keeps this function total. Deliberately different
            // from Type_SizeOf's void case (0 bytes of STORAGE), not an accidental split.
            return w >= 0 ? w : 8;
        }
        case TYPE_POINTER:   return 8;
        case TYPE_ARRAY:     return 8; // value-as-address in register context (for now)
        case TYPE_STRUCT:    return 8; // address in register context (aggregates not in regs yet)
        case TYPE_FUNCTION:  return 8; // pointer to function
        case TYPE_FN_LITERAL: return 8; // same runtime representation as TYPE_FUNCTION
        case TYPE_PARAM:     return 8;
    }
    return 8;
}

// --- small constructors ---

static Type* make_prim(PrimitiveKind p) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_PRIMITIVE;
    t->primitive = p;
    return t;
}

static Type* make_ptr(Type* base) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    t->cls = TYPE_POINTER;
    t->pointer_base = base;
    return t;
}

static bool is_untyped_int_literal(ASTNode* n);
static bool is_untyped_float_literal(ASTNode* n);
static bool is_null_literal(ASTNode* n);
static bool is_untyped_literal(ASTNode* n);
static bool int_fits(int64_t v, const Type* dst);
static bool check_assignable_ne(Type* dst, ASTNode* src, const char* where);
static bool unify_types(Type* concrete, Type* generic, const char** type_params, Type** inferred_args, size_t param_count);

// Usual-arithmetic-conversion (lite, for v1 integers):
// result is the operand with the larger width; on a tie, unsigned wins.
//
// node_a/node_b are the ORIGINAL operand AST nodes (may be NULL if a caller has
// no node, e.g. a synthetic type-only check) — needed so a bare untyped literal
// (`10`, not `(i32)10`) can defer to the OTHER operand's real type instead of
// silently dragging the result up to the literal's arbitrary i32/i64 default.
// Without this, `u8 x; x + 10` infers as i32 (the literal's default just
// happens to be wider than u8), which is exactly the "endemic per-path" bug
// class coerce_literal_to_target above already exists to centralize away from —
// this is that same rule's one remaining call site.
static Type* arith_result(Type* a, Type* b, ASTNode* node_a, ASTNode* node_b) {
    if (!a) return b;
    if (!b) return a;
    // Pointer arithmetic: previously fell through to the width-comparison path
    // below by accident (a pointer's storage width just happened to dominate
    // most integer operand widths), which gave the right type for ptr+int but
    // the WRONG type for ptr-ptr (returned a pointer instead of a distance).
    // Made explicit here so it's correct by design, not by coincidence:
    //   ptr - ptr  -> i64 (element distance; codegen divides by pointee size)
    //   ptr +/- int -> the pointer type (codegen scales the int by pointee size)
    if (a->cls == TYPE_POINTER || b->cls == TYPE_POINTER) {
        // void* (PRIM_V pointee) has no defined element size — Type_SizeOf
        // returns 0 for it, which would make `void* + N` scale by zero and
        // silently collapse to `void* + 0` for any N. That's exactly the
        // "hidden machinery" the language is supposed to refuse to do
        // quietly, so reject it outright instead (matches C, which makes
        // void* arithmetic illegal rather than defining it as byte-stride).
        Type* ptr_a = (a->cls == TYPE_POINTER) ? a : NULL;
        Type* ptr_b = (b->cls == TYPE_POINTER) ? b : NULL;
        if ((ptr_a && ptr_a->pointer_base && ptr_a->pointer_base->cls == TYPE_PRIMITIVE &&
             ptr_a->pointer_base->primitive == PRIM_V) ||
            (ptr_b && ptr_b->pointer_base && ptr_b->pointer_base->cls == TYPE_PRIMITIVE &&
             ptr_b->pointer_base->primitive == PRIM_V)) {
            Error_AtNode(node_a ? node_a : node_b, "pointer arithmetic on void* is not allowed "
                            "(void* has no element size to scale by — cast to a "
                            "concrete pointer type first)", NULL);
        }
    }
    if (a->cls == TYPE_POINTER && b->cls == TYPE_POINTER) {
        return make_prim(PRIM_I64);
    }
    if (a->cls == TYPE_POINTER) return a;
    if (b->cls == TYPE_POINTER) return b;
    // Function-pointer arithmetic: TYPE_FUNCTION is a distinct class from
    // TYPE_POINTER (not the pointer branch above), and Type_Width(TYPE_FUNCTION)
    // is 8 — same as every pointer — so this used to fall straight through to
    // the width-comparison path at the bottom and silently "succeed" with some
    // 8-byte result type. codegen then computed garbage (raw address + N, no
    // meaningful stride) and calling the resulting "function pointer" segfaulted
    // at runtime. Same reasoning as the void* guard above: reject at the type
    // level instead of letting it compile into undefined behavior.
    if (a->cls == TYPE_FUNCTION || b->cls == TYPE_FUNCTION) {
        Error_AtNode(node_a ? node_a : node_b, "arithmetic on a function pointer is not allowed "
                        "(a function pointer has no element size to scale by)", NULL);
    }
    // Literal-defers-to-context: only when exactly one side is a bare literal
    // and it actually fits the other side's type (an oversized literal still
    // promotes normally — deferring would silently truncate it). Placed after
    // the pointer checks above so it can never mask void*-arithmetic rejection
    // or interfere with pointer-vs-pointer/pointer-vs-int typing — by this
    // point both a and b are known-non-pointer, so this only ever governs
    // scalar-vs-scalar results.
    bool lit_a = is_untyped_int_literal(node_a);
    bool lit_b = is_untyped_int_literal(node_b);
    if (lit_a && !lit_b && int_fits(node_a->int_value, b)) return b;
    if (lit_b && !lit_a && int_fits(node_b->int_value, a)) return a;
    // Floating point dominates: if either side is float, the result is float
    // (f64 if either is f64, else f32). No implicit int<->float mixing beyond
    // this promotion — that matches the usual-arithmetic-conversion shape.
    bool af = Type_IsFloat(a);
    bool bf = Type_IsFloat(b);
    if (af || bf) {
        bool a64 = af && a->primitive == PRIM_F64;
        bool b64 = bf && b->primitive == PRIM_F64;
        return make_prim((a64 || b64) ? PRIM_F64 : PRIM_F32);
    }
    int wa = Type_Width(a), wb = Type_Width(b);
    if (wa > wb) return a;
    if (wb > wa) return b;
    // equal width: unsigned wins (matches C rank rule)
    if (!Type_IsSigned(a)) return a;
    return b;
}

static bool is_untyped_int_literal(ASTNode* n);

// THE single home for "an untyped scalar literal adapts to its target type."
// Every site that places a literal into a typed slot (declaration, assignment,
// return, function arg — direct or via fnptr, struct field, array element, and the
// constexpr equivalents) routes through here. Centralizing it is the fix for the
// "endemic" class of bugs where the same coercion had to be re-implemented per
// path and one path would silently miss it (e.g. int literal -> float slot stored
// as integer bits = ~0). The rewrite is IN PLACE on the AST node.
//
// Currently handles: untyped int literal -> float target (rewrite to a float
// literal so codegen emits the IEEE pattern). Other tiers (null->ptr, int width
// fit) remain validation in check_assignable; this owns the value-changing rewrite.
// Returns true if it rewrote the node.
static bool coerce_literal_to_target(ASTNode* node, Type* target) {
    if (!node || !target) return false;
    if (is_untyped_int_literal(node) && Type_IsFloat(target)) {
        node->lit_kind = LIT_FLOAT;
        node->float_value = (double)node->int_value;
        return true;
    }
    return false;
}

// Forward declaration — resolve_brace_literal's scalar-leaf case needs this to
// validate (not just rewrite) a literal landing in a primitive/pointer slot,
// exactly like every other literal-placement site (decl, assignment, return,
// call arg) already does. Without this, an aggregate literal's leaves skipped
// straight to coerce_literal_to_target (the value-changing rewrite only) and
// never hit the fit/kind checks that check_assignable owns, so e.g. a float
// literal in an i32 field raw-stored its bits instead of erroring.
static void check_assignable(Type* dst, ASTNode* src, const char* where);

// Forward declaration — resolve_brace_literal needs this for struct-field /
// array-element values that are themselves unresolved generic references
// (a call to a generic function, or a bare name referring to one).
// Declared in compiler.h now (no longer static): ConstEval in constexpr.c
// calls this directly so a const-folded generic call gets the identical
// argument-based inference the ordinary typecheck pass uses.

// Resolve a bare (untyped) aggregate literal `{...}` against its target type. The
// parser emits struct-shaped literals as AST_STRUCT_LITERAL with sdef==NULL and
// array-shaped as AST_ARRAY_LITERAL with elem_type==NULL; this fills them in from
// the context type (decl, return, param, field, element). Recurses for nested
// literals (a field/element value that is itself a bare `{...}`). No-op if the
// node is already resolved or isn't a bare literal.
void resolve_brace_literal(ASTNode* node, Type* target) {
    if (!node || !target) return;

    // Scalar literal leaf adapting to a typed slot — the one coercion path.
    if (coerce_literal_to_target(node, target)) return;

    // Deferred contextual enum literal `.Variant{payload}` (sdef==NULL, flagged):
    // resolve the enum type from the target, validate the variant, and resolve the
    // payload against the variant's field type. Mirrors the eager
    // `EnumType.Variant{..}` path; this is the only enum-specific resolver line.
    if (node->type == AST_STRUCT_LITERAL && node->struct_lit.sdef == NULL
        && node->struct_lit.is_enum_variant) {
        if (target->cls != TYPE_STRUCT)
            Error_AtNode(node, "enum literal `.Variant` used where a non-enum type is expected", NULL);
        StructDef* sd = Struct_Find(target->struct_name);
        if (!sd || !sd->is_enum) {
            char msg[256];
            snprintf(msg, sizeof(msg), "enum literal `.%.*s` used where %s is not an enum",
                     (int)node->struct_lit.field_name_lens[0], node->struct_lit.field_names[0],
                     target->struct_name ? target->struct_name : "the target type");
            Error_AtNode(node, msg, NULL);
        }
        StructField* variant = Struct_FindField(sd, node->struct_lit.field_names[0],
                                                node->struct_lit.field_name_lens[0]);
        if (!variant) {
            char msg[256];
            snprintf(msg, sizeof(msg), "enum %s has no variant %.*s", sd->name,
                     (int)node->struct_lit.field_name_lens[0], node->struct_lit.field_names[0]);
            Error_AtNode(node, msg, NULL);
        }
        node->struct_lit.sdef = sd;
        if (node->struct_lit.count == 1 && variant->type) {
            resolve_brace_literal(node->struct_lit.values[0], variant->type);
            infer_generic(node->struct_lit.values[0], variant->type);
        }
        node->result_type = target;
        return;
    }

    if (node->type == AST_STRUCT_LITERAL && node->struct_lit.sdef == NULL) {
        if (target->cls != TYPE_STRUCT)
            Error_AtNode(node, "struct literal `{.field=..}` used where a non-struct type is expected", NULL);
        StructDef* sd = Struct_Find(target->struct_name);
        if (!sd) Error_AtNode(node, "unknown struct type for literal", NULL);
        // A designated literal `{.Variant = payload}` targeting an enum resolves
        // here same as an ordinary struct field: enum variants are stored as
        // StructFields on their StructDef, so Struct_FindField below finds the
        // variant exactly like it would a real field. This IS the enum
        // construction idiom -- `{.Some = 30}` -- not a coincidence to guard
        // against.
        node->struct_lit.sdef = sd;
        // Validate each named field exists; recurse so nested `{...}` field values
        // resolve against the field's declared type.
        for (size_t i = 0; i < node->struct_lit.count; i++) {
            StructField* f = Struct_FindField(sd, node->struct_lit.field_names[i],
                                              node->struct_lit.field_name_lens[i]);
            if (!f) {
                char msg[256];
                snprintf(msg, sizeof(msg), "%s %s has no field %.*s", sd->is_overlapping ? "union" : "struct", sd->name,
                         (int)node->struct_lit.field_name_lens[i], node->struct_lit.field_names[i]);
                Error_AtNode(node, msg, NULL);
            }
            resolve_brace_literal(node->struct_lit.values[i], f->type);
            infer_generic(node->struct_lit.values[i], f->type);
            // resolve_brace_literal only REWRITES a literal that fits (int->float);
            // a literal that doesn't fit (float->int, out-of-range int, null->non-ptr)
            // falls through unchanged and must still be validated here, exactly like
            // every other literal-placement site (decl/assign/return/call arg) already
            // is via check_assignable — otherwise it silently reaches codegen with its
            // raw bits reinterpreted as the field's type instead of erroring. Gated to
            // literal leaves only: this same resolver walk also runs over match/unpack
            // DESTRUCTURE patterns (`{.x = a, .y = b}`), where a field "value" is really
            // a bare bind identifier (possibly `_`) with no symbol yet — check_assignable
            // would wrongly see that as an unresolved-type value and error "void vs value".
            ASTNode* fv = node->struct_lit.values[i];
            if (is_untyped_literal(fv)) {
                check_assignable(f->type, fv, "struct field");
            }
        }
        node->result_type = target;
        return;
    }

    if (node->type == AST_ARRAY_LITERAL && node->array_lit.elem_type == NULL) {
        // Zero-initialization of scalar targets: `(i32){}` or `(ptr){}` becomes `0`.
        // This is crucial for generic bodies that use `(T){}` to zero-initialize any T.
        if ((target->cls == TYPE_PRIMITIVE || target->cls == TYPE_POINTER) && node->array_lit.count == 0) {
            node->type = AST_INT_LITERAL;
            node->int_value = 0;
            node->lit_kind = LIT_INT; // untyped zero, adapts to target
            node->result_type = target;
            return;
        }

        // Positional struct literal: a bare `{a, b, c}` (array-shaped at parse time,
        // since it has no `.field=`) targeting a STRUCT means "fill the fields in
        // declaration order". The shape isn't known until the target is — exactly
        // the language's "literals carry no intrinsic type" rule — so we resolve it
        // here, where target is known, and convert the node IN-PLACE to a struct
        // literal (synthesizing field names in order) so codegen uses field offsets,
        // not array stride. Then fall through to the struct-literal resolver, so the
        // positional values recurse against each field's type (nesting composes).
        if (target->cls == TYPE_STRUCT) {
            StructDef* sd = Struct_Find(target->struct_name);
            if (!sd) Error_AtNode(node, "unknown struct type for literal", NULL);
            if (sd->is_enum) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "positional literal `{..}` cannot construct enum %s; "
                         "use `.Variant{..}` or %s.Variant{..}", sd->name, sd->name);
                Error_AtNode(node, msg, NULL);
            }
            if (node->array_lit.count > sd->field_count) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "positional literal for struct %s has %zu values, expected %zu fields",
                         sd->name, node->array_lit.count, sd->field_count);
                Error_AtNode(node, msg, NULL);
            }
            // Reinterpret the array node as a struct literal in declaration order.
            // Fewer values than fields is fine — missing fields get zero-initialized.
            size_t n = sd->field_count;
            size_t provided = node->array_lit.count;
            ASTNode** orig_vals = node->array_lit.values;
            ASTNode** vals = (ASTNode**)malloc(n * sizeof(ASTNode*));
            for (size_t i = 0; i < provided; i++) vals[i] = orig_vals[i];
            // Un-provided fields are handled by codegen: pre-zero covers no-default fields,
            // and has_default fields are stamped from default_val_buf. Don't synthesize
            // explicit zeros — they would overwrite defaults. Only emit entries for
            // the values that were actually provided (positional order).
            node->type = AST_STRUCT_LITERAL;
            node->struct_lit.sdef = NULL;              // set by the struct resolver below
            node->struct_lit.is_enum_variant = false;
            node->struct_lit.count = provided;
            node->struct_lit.values = vals;
            node->struct_lit.field_names = (const char**)malloc((n ? n : 1) * sizeof(char*));
            node->struct_lit.field_name_lens = (size_t*)malloc((n ? n : 1) * sizeof(size_t));
            for (size_t i = 0; i < n; i++) {
                node->struct_lit.field_names[i] = sd->fields[i].name;
                node->struct_lit.field_name_lens[i] = strlen(sd->fields[i].name);
            }
            // Now an AST_STRUCT_LITERAL with sdef==NULL: re-run the resolver so the
            // struct branch above fills sdef and recurses into each positional value.
            resolve_brace_literal(node, target);
            return;
        }
        if (target->cls != TYPE_ARRAY)
            Error_AtNode(node, "array literal `{..}` used where a non-array type is expected", NULL);
        Type* elem = target->array.element;
        node->array_lit.elem_type = elem;
        // Resolve declared-vs-actual count: target may have a fixed N or be inferred [].
        // An empty {} against a known-size array is zero-init: synthesize N zero literals.
        if (target->array.count != 0 && node->array_lit.count == 0) {
            size_t n = (size_t)target->array.count;
            node->array_lit.values = (ASTNode**)realloc(node->array_lit.values, n * sizeof(ASTNode*));
            for (size_t zi = 0; zi < n; zi++) {
                ASTNode* z = (ASTNode*)calloc(1, sizeof(ASTNode));
                z->type = AST_INT_LITERAL;
                z->int_value = 0;
                z->lit_kind = LIT_INT; // untyped zero, adapts to element type
                node->array_lit.values[zi] = z;
            }
            node->array_lit.count = n;
        } else if (target->array.count != 0 && target->array.count != node->array_lit.count) {
            char msg[128];
            snprintf(msg, sizeof(msg), "array literal has %zu elements, expected %llu",
                     node->array_lit.count, (unsigned long long)target->array.count);
            Error_AtNode(node, msg, NULL);
        }
        // Inferred size `u32[]`: write the literal's element count back into the
        // (shared) declared type, so sizeof/allocation see the real size. (Was the
        // crash: count stayed 0 -> 0-byte alloc, then writing elements overran it.)
        if (target->array.count == 0 && !target->array.count_expr)
            target->array.count = node->array_lit.count;
        // Recurse: an element may itself be a bare `{...}` of the element type,
        // or a generic function name that should monomorphize to the element type.
        for (size_t i = 0; i < node->array_lit.count; i++) {
            resolve_brace_literal(node->array_lit.values[i], elem);
            infer_generic(node->array_lit.values[i], elem);
            // See the struct-field case above: validate literals resolve_brace_literal
            // left unrewritten (wrong-direction or out-of-range) instead of letting them
            // silently reach codegen with mismatched bits. Gated to literal leaves only
            // -- this walk also runs over match/unpack array-destructure patterns, whose
            // elements may be bare bind identifiers (or `_`), not values.
            ASTNode* ev = node->array_lit.values[i];
            if (is_untyped_literal(ev)) {
                check_assignable(elem, ev, "array element");
            }
        }
        if (target->array.count_expr) {
            Type* concrete = (Type*)malloc(sizeof(Type));
            *concrete = *target;
            concrete->array.count_expr = NULL;
            concrete->array.count = node->array_lit.count;
            node->result_type = concrete;
        } else {
            node->result_type = target;
        }
        return;
    }
    // Already-resolved literal or any other node: nothing to do.
}




// Save/install/restore the comptime generic-substitution frame ConstEval
// consults mid-fold.
typedef struct { const char** params; Type** args; size_t n; } CeGenericFrame;
static CeGenericFrame ce_generic_frame_install(const char** params, Type** args, size_t n) {
    CeGenericFrame saved = { s_ce_generic_params, s_ce_generic_args, s_ce_generic_n };
    s_ce_generic_params = params; s_ce_generic_args = args; s_ce_generic_n = n;
    return saved;
}
static void ce_generic_frame_restore(CeGenericFrame saved) {
    s_ce_generic_params = saved.params; s_ce_generic_args = saved.args; s_ce_generic_n = saved.n;
}

// Fold one array-postfix count_expr under a substitution frame: ConstEval,
// positive check, else defer. Shared by Type_Substitute's own TYPE_ARRAY
// case and array_substitute_maybe_hkt's outer-dimension rebuild.
static void fold_array_count(ASTNode* ce, const char** params, Type** args, size_t n,
                              uint64_t* count_out, ASTNode** count_expr_out) {
    if (!ce) { *count_expr_out = NULL; return; }
    int64_t eval_val;
    CeGenericFrame saved = ce_generic_frame_install(params, args, n);
    bool ok = ConstEval(ce, &eval_val);
    ce_generic_frame_restore(saved);
    if (ok) {
        if (eval_val <= 0) {
            fprintf(stderr, "Error: array size must be positive (const-generic size evaluated to %lld)\n",
                    (long long)eval_val);
            exit(1);
        }
        *count_out = (uint64_t)eval_val;
        *count_expr_out = NULL;
    } else {
        *count_expr_out = ce;
    }
}

// One array-postfix dimension as a Type* type argument, or NULL if it's an
// ordinary (non-type) array-size expression.
static Type* count_expr_as_type_arg(ASTNode* count_expr, const char** params, Type** args, size_t n) {
    if (!count_expr) return NULL;
    if (count_expr->type == AST_TYPE_EXPR) {
        return Type_Substitute(count_expr->sizeof_expr.type, params, args, n);
    }
    if (count_expr->type == AST_IDENT) {
        for (size_t i = 0; i < n; i++) {
            if (strcmp(count_expr->ident.name, params[i]) == 0) return args[i];
        }
    }
    return NULL;
}

// `M[T][N]`, M an unapplied template: leftmost bracket is outermost (same
// fold as u32[2][3]), so T (M's type arg) wraps N (a real array dim) --
// backwards from what M[T] needs. Consumes `arity` brackets from the
// outside in as type args; whatever's left wraps the instantiated result
// as ordinary array dims. NULL (no-op) unless the base is confirmed a
// still-generic template -- T[N]/Mat[T,R,C]{T[R][C]} never reach that.
static Type* array_substitute_maybe_hkt(Type* t, const char** params, Type** args, size_t n) {
    size_t depth = 0;
    Type* base = t;
    for (; base->cls == TYPE_ARRAY; base = base->array.element) depth++;
    if (depth == 0) return NULL;

    ASTNode** count_exprs = (ASTNode**)malloc(depth * sizeof(ASTNode*));
    size_t i = 0;
    for (Type* cur = t; cur->cls == TYPE_ARRAY; cur = cur->array.element) count_exprs[i++] = cur->array.count_expr;
    Type* base_sub = Type_Substitute(base, params, args, n);

    if (!base_sub || base_sub->cls != TYPE_STRUCT) { free(count_exprs); return NULL; }
    StructDef* esd = Struct_Find(base_sub->struct_name);
    if (!esd || !esd->is_generic || esd->type_param_count == 0) { free(count_exprs); return NULL; }
    if (esd->type_param_count > depth) {
        // A bare, deliberately-unapplied template (struct_unapplied, from an
        // HKT slot) with FEWER brackets than its arity is under-applied -- an
        // error, not a silent fallthrough that leaves it unapplied and
        // zero-sized. An ordinary generic struct named as an array element
        // (not struct_unapplied) is genuinely not an HKT here; leave it alone.
        if (base_sub->struct_unapplied) {
            fprintf(stderr, "Error: generic struct '%s' expects %zu type arguments, "
                            "but is applied to only %zu here\n",
                            esd->name, esd->type_param_count, depth);
            exit(1);
        }
        free(count_exprs); return NULL;
    }

    size_t arity = esd->type_param_count;
    Type** type_args = (Type**)malloc(arity * sizeof(Type*));
    for (size_t k = 0; k < arity; k++) {
        Type* ta = count_expr_as_type_arg(count_exprs[k], params, args, n);
        if (!ta) {
            // A confirmed template short on type-shaped brackets (e.g. M[T][N]
            // with M arity-2 and N a real value) is an arity error, not a
            // silent fallthrough to "unapplied" -- that used to read as
            // sizeof 0 instead of failing.
            fprintf(stderr, "Error: generic struct '%s' expects %zu type arguments "
                            "(used as %s[...] here), got fewer -- the remaining "
                            "bracket(s) are not type-shaped\n",
                            esd->name, arity, esd->name);
            exit(1);
        }
        type_args[k] = ta;
    }
    StructDef* inst = Struct_Instantiate(esd, type_args, arity);
    free(type_args);

    // Rebuild any remaining OUTER-of-the-consumed-arity levels as genuine
    // arrays (the brackets nearer the front that weren't needed for arity --
    // for `M[T][N]` with a 1-ary M, arity consumes count_exprs[0]=T, leaving
    // count_exprs[1]=N to wrap the instantiated result as a real array dim).
    Type* result = (Type*)calloc(1, sizeof(Type));
    result->cls = TYPE_STRUCT;
    result->struct_name = inst->name;
    for (size_t k = depth; k > arity; k--) {
        Type* outer = (Type*)calloc(1, sizeof(Type));
        outer->cls = TYPE_ARRAY;
        outer->array.element = result;
        fold_array_count(count_exprs[k - 1], params, args, n, &outer->array.count, &outer->array.count_expr);
        result = outer;
    }
    free(count_exprs);
    return result;
}

// --- type substitution ---
Type* Type_Substitute(Type* t, const char** params, Type** args, size_t n) {
    if (!t) return NULL;
    if (t->cls == TYPE_PARAM) {
        for (size_t i = 0; i < n; i++) {
            if (t->param_name && strcmp(t->param_name, params[i]) == 0) {
                return args[i]; // replace with concrete type
            }
        }
        return t; // unbound param
    }
    if (t->cls == TYPE_CONST_VALUE) {
        if (!t->cval.defer) return t; // already concrete
        // Deferred value arg (e.g. `M * 2` from an outer generic). Re-fold with the
        // frame active so its params resolve, then produce a concrete value pinned
        // to the same type. Mirrors the array.count_expr deferral below.
        CeGenericFrame saved = ce_generic_frame_install(params, args, n);
        Type* pin = t->cval.pin;
        bool aggregate = Type_IsAggregate(pin);
        Type* r = (Type*)calloc(1, sizeof(Type));
        r->cls = TYPE_CONST_VALUE;
        r->cval.pin = pin;
        if (aggregate) {
            int64_t off = ConstEval_AggPersist(t->cval.defer, pin);
            if (off >= 0) { r->cval.is_agg = true; r->cval.agg_off = (uint32_t)off; }
            else          { r->cval.defer = t->cval.defer; } // still unresolved (deeper nesting)
        } else {
            int64_t v;
            if (ConstEval(t->cval.defer, &v)) { r->cval.scalar = v; }
            else                              { r->cval.defer = t->cval.defer; }
        }
        ce_generic_frame_restore(saved);
        return r;
    }
    Type* c = (Type*)calloc(1, sizeof(Type));
    *c = *t;
    if (t->cls == TYPE_POINTER) c->pointer_base = Type_Substitute(t->pointer_base, params, args, n);
    else if (t->cls == TYPE_ARRAY) {
        // M[T][N] on an unapplied template: see array_substitute_maybe_hkt.
        Type* hkt = array_substitute_maybe_hkt(t, params, args, n);
        if (hkt) return hkt;

        c->array.element = Type_Substitute(t->array.element, params, args, n);
        if (t->array.count_expr)
            fold_array_count(t->array.count_expr, params, args, n, &c->array.count, &c->array.count_expr);
    }
    else if (t->cls == TYPE_STRUCT && t->struct_name) {
        StructDef* sd = Struct_Find(t->struct_name);
        if (sd && sd->generic_base) {
            // Already an instantiation: re-instantiate with substituted args
            Type** new_args = malloc(sd->type_arg_count * sizeof(Type*));
            for (size_t i = 0; i < sd->type_arg_count; i++) {
                new_args[i] = Type_Substitute(sd->type_args[i], params, args, n);
            }
            StructDef* new_sd = Struct_Instantiate(sd->generic_base, new_args, sd->type_arg_count);
            c->struct_name = new_sd->name;
            free(new_args);
        } else if (sd && sd->is_generic && sd->type_param_count > 0 && !t->struct_unapplied) {
            // Template struct (e.g. Box): if any of its type_params are in scope,
            // instantiate it with the substituted args. This handles `Box*` self
            // params (self's implicit type is a bare `TYPE_STRUCT{struct_name=Box}`
            // built fresh per method, so it never shares array identity with
            // whatever frame substitutes it -- name-matching against the CURRENT
            // frame's params is the only signal available here).
            //
            // struct_unapplied (set only by the "let an unapplied template ride
            // through" call-site path) opts a node OUT of this: without it, a bare
            // Box used as a foreign generic's ARGUMENT (`describe[M,T](...)` binding
            // M=Box) falsely matched this branch whenever Box's OWN param name
            // ("T") happened to collide with an unrelated same-named param in
            // describe's frame, force-instantiating Box into the wrong concrete
            // type using describe's T instead of leaving it deliberately unapplied.
            bool any_match = false;
            for (size_t i = 0; i < sd->type_param_count && !any_match; i++)
                for (size_t j = 0; j < n && !any_match; j++)
                    if (strcmp(sd->type_params[i], params[j]) == 0) any_match = true;
            if (any_match) {
                Type** new_args = malloc(sd->type_param_count * sizeof(Type*));
                for (size_t i = 0; i < sd->type_param_count; i++) {
                    Type* tp = (Type*)calloc(1, sizeof(Type));
                    tp->cls = TYPE_PARAM; tp->param_name = sd->type_params[i];
                    new_args[i] = Type_Substitute(tp, params, args, n);
                    free(tp);
                }
                StructDef* new_sd = Struct_Instantiate(sd, new_args, sd->type_param_count);
                c->struct_name = new_sd->name;
                free(new_args);
            }
        } else if (sd && sd->is_anonymous && sd->field_count > 0) {
            // Anonymous (content-named) struct: it has no generic_base and isn't a
            // template, so the two branches above skip it -- but its FIELD TYPES may
            // still contain TYPE_PARAMs being substituted (e.g. an `alias Pair[T] =
            // struct { T a  T b }`, or an anon struct built from a captured `match T`
            // wildcard). Substitute each field type and re-register under the new
            // content-derived name, mirroring exactly how the parser first builds an
            // anon struct. Structural identity == name identity, so the substituted
            // result dedups against a hand-written `struct { i32 a  i32 b }` for free.
            size_t fc = sd->field_count;
            Type** newf = (Type**)malloc(fc * sizeof(Type*));
            bool any = false;
            for (size_t i = 0; i < fc; i++) {
                newf[i] = Type_Substitute(sd->fields[i].type, params, args, n);
                if (newf[i] != sd->fields[i].type) any = true;
            }
            if (any) {
                // Preserve is_enum/is_overlapping in both the name (so a substituted
                // enum pattern can never dedupe-collide with a struct/union of the
                // same field shape -- same reasoning as Struct_MakeAnon's kind-prefixed
                // name) and on the re-registered StructDef itself. Without this, an
                // `enum { u32 h }` pattern used inside a GENERIC function silently
                // lost its is_enum flag the moment substitution ran (even with zero
                // actual TYPE_PARAMs left to substitute, `any` can still be true via
                // e.g. a wildcard-bound field), and reflect_unify's
                // `cs->is_enum == ps->is_enum` gate then always failed against a real
                // enum concrete type -- a concrete-typed enum pattern (`u32 h`) never
                // matched anything, while a WILDCARD enum pattern (`H h`) happened to
                // take a different path earlier and worked. Same class of bug already
                // fixed once for Struct_MakeAnon; this is the substitution-time sibling.
                const char* kw = sd->is_enum ? "enum{" : (sd->is_overlapping ? "union{" : "struct{");
                char namebuf[1024]; size_t off = 0;
                off += snprintf(namebuf + off, sizeof(namebuf) - off, "%s", kw);
                for (size_t i = 0; i < fc; i++) {
                    char tn[128]; Type_ToString(newf[i], tn, sizeof(tn));
                    off += snprintf(namebuf + off, sizeof(namebuf) - off, "%s%s%s",
                                    (int)i == sd->pack_field_index ? "..." : "", tn,
                                    (i + 1 < fc) ? "," : "");
                }
                snprintf(namebuf + off, sizeof(namebuf) - off, "}");
                StructDef* nsd = Struct_Register(namebuf, strlen(namebuf));
                if (nsd->field_count == 0 && !nsd->laid_out) {
                    nsd->is_enum = sd->is_enum;
                    nsd->is_overlapping = sd->is_overlapping;
                    nsd->is_anonymous = true;
                    nsd->pack_field_index = sd->pack_field_index;
                    nsd->fields = (StructField*)calloc(fc, sizeof(StructField));
                    nsd->field_count = fc;
                    for (size_t i = 0; i < fc; i++) {
                        nsd->fields[i].name = sd->fields[i].name;
                        nsd->fields[i].type = newf[i];
                        nsd->fields[i].offset = 0;
                        nsd->fields[i].variant_tag = sd->fields[i].variant_tag;
                    }
                }
                c->struct_name = nsd->name;
            }
            free(newf);
        }
    }
    else if (t->cls == TYPE_FUNCTION) {
        c->function.return_type = Type_Substitute(t->function.return_type, params, args, n);
        if (t->function.param_count) {
            c->function.param_types = malloc(t->function.param_count * sizeof(Type*));
            for (size_t i = 0; i < t->function.param_count; i++)
                c->function.param_types[i] = Type_Substitute(t->function.param_types[i], params, args, n);
        }
    }
    else if (t->cls == TYPE_IMPL) {
        // A generic `alias A[U] = impl { fn set(U) ... }`: substitute U into each
        // method signature. Without this, U stayed an unbound wildcard and the
        // impl-pattern matched ANY type (see reflect_unify's is_hole) instead of
        // only types with the U-substituted methods. sigs[] are TYPE_FUNCTIONs,
        // already substitutable; names are shared verbatim (not owned per-copy).
        if (t->impl_pat.method_count) {
            c->impl_pat.sigs = malloc(t->impl_pat.method_count * sizeof(Type*));
            for (size_t i = 0; i < t->impl_pat.method_count; i++)
                c->impl_pat.sigs[i] = Type_Substitute(t->impl_pat.sigs[i], params, args, n);
        }
    }
    return c;
}

// Shared guard + substitution for "a type expressed in terms of a generic
// struct's OWN type params, resolved through a concrete instantiation of
// that struct" -- the same one-line check (generic_base set, at least one
// real type arg) and the same Type_Substitute call were independently
// duplicated at three call sites (a method's return type, an __assign
// operand's param type, a for-in cursor's begin()/next() return types)
// before being pulled out here. `sd` non-generic or non-instantiated
// (generic_base NULL, or type_arg_count == 0) is the common case and
// returns `t` unchanged, same as before.
Type* Type_Substitute_Through_Instance(Type* t, StructDef* sd) {
    if (t && sd && sd->generic_base && sd->type_arg_count > 0) {
        return Type_Substitute(t, sd->generic_base->type_params,
                               sd->type_args, sd->type_arg_count);
    }
    return t;
}

// impl method call: `expr.method(args)` parses as AST_CALL with target_expr=AST_FIELD.
// If `method` isn't an actual field of expr's struct, look up the mangled
// `BaseStructName_method` symbol (impl desugars to this naming convention) and
// rewrite the node in place: target_expr -> NULL, sym -> the mangled function,
// self injected as args[0] (address-of if expr is a value, as-is if already a
// pointer). For an instantiated generic struct (Box$u32), the mangled lookup
// uses the generic base's name (Box) and the call's type_args are set from the
// struct's type_args so the method monomorphizes with the right concrete types.
// Idempotent: a no-op if target_expr is already NULL or not AST_FIELD, so it's
// safe to call from both Type_Infer and Typecheck_Tree's AST_CALL cases.
// The ONE place a (type, method-name) pair becomes a symbol name. Three callers now
// need this exact string -- try_rewrite_method_call (`p.free()`), reflect_unify
// (`impl { fn free() }`), and Method_Resolve below (`fnof(T, free)`) -- and they must
// agree, or a method would be callable but not matchable, or matchable but not
// addressable.
//
// Note it is NOT `nameof(T) ++ "_" ++ name`: for an INSTANTIATED generic the methods
// live under the GENERIC BASE (`Vector[i32]`'s free is `Vector_free`, not
// `Vector_i32_free`), so the type must be resolved through generic_base first. That is
// exactly why `fnof` cannot be built out of `nameof` and string concatenation in user
// code, and why it has to be a compiler-side resolution.
char* Method_Mangle(const Type* t, const char* mname, size_t mlen, size_t* out_len) {
    const Type* st = t;
    if (st && st->cls == TYPE_POINTER && st->pointer_base) st = st->pointer_base;
    if (!st || st->cls != TYPE_STRUCT || !st->struct_name) return NULL;
    StructDef* sd = Struct_Find(st->struct_name);
    const char* base_name = (sd && sd->generic_base) ? sd->generic_base->name : st->struct_name;
    size_t blen = strlen(base_name);
    size_t manglen = blen + 1 + mlen;
    char* mangled = (char*)malloc(manglen + 1);
    memcpy(mangled, base_name, blen);
    mangled[blen] = '_';
    memcpy(mangled + blen + 1, mname, mlen);
    mangled[manglen] = '\0';
    if (out_len) *out_len = manglen;
    return mangled;
}

// `fnof(T, m)` -> the Symbol for T's method `m`, or NULL if T has no such method.
// A pure lookup: no new machinery, just the resolution try_rewrite_method_call already
// performs, exposed so an EXPRESSION can name the method instead of only CALLING it.
Symbol* Method_Resolve(const Type* t, const char* mname, size_t mlen) {
    size_t manglen = 0;
    char* mangled = Method_Mangle(t, mname, mlen, &manglen);
    if (!mangled) return NULL;
    Symbol* s = SymTable_Find(Get_SymTable(), mangled, manglen);
    free(mangled);
    if (!s || s->kind != SYM_FUNCTION) return NULL;
    return s;
}

// Call-operator overload: `a(x)` where `a` is a struct (or pointer-to-struct)
// VALUE, not a function, desugars to `a.__call(x)` if the struct defines that
// method -- same sugar trick as s_op_methods below (a + b -> a.__add(b)), just
// for AST_CALL instead of a binary node. Rewrites target_expr into the
// AST_FIELD shape try_rewrite_method_call already knows how to mangle/
// self-inject, then defers to it. Returns true if it rewrote the node (caller
// must re-run its own type-inference/typecheck on `node` afterward); false
// (no-op) if `tgt` isn't a struct or has no __call method, so the caller falls
// through to its ordinary "calling non-function" error unchanged.
// Not static -- ConstEval needs this too, same reason as
// try_rewrite_operator_method (see that function's own comment).
bool try_rewrite_call_operator(ASTNode* node, Type* tgt) {
    // Method_Resolve (not Struct_FindField -- see the s_op_methods dispatch
    // fix above for the full story) already unwraps pointer-to-struct and
    // resolves through a generic base, so no manual unwrap is needed here.
    if (!tgt || !Method_Resolve(tgt, "__call", 6)) return false;
    ASTNode* field = (ASTNode*)calloc(1, sizeof(ASTNode));
    field->type = AST_FIELD;
    field->field.base = node->call.target_expr;
    field->field.field_name = "__call";
    field->field.field_name_len = 6;
    field->line = node->line;
    field->column = node->column;
    node->call.target_expr = field;
    return true;
}

void try_rewrite_method_call(ASTNode* node) {
    if (!node->call.target_expr || node->call.target_expr->type != AST_FIELD) return;
    ASTNode* field_node = node->call.target_expr;
    // The receiver may itself be an unresolved generic method call (chained calls
    // on a generic return, e.g. `bb.get().get()`). The inner call needs its own
    // method-call rewrite (to set .sym = Box_get and populate self_type_args) AND
    // infer_generic (to substitute self_type_args into .type_args) BEFORE we infer
    // its type here — otherwise Type_Infer sees the raw unsubstituted TYPE_PARAM T
    // (not a struct), this outer rewrite silently bails, and the call falls through
    // to AST_FIELD's hard error ("has no field 'method'").
    if (field_node->field.base->type == AST_CALL) {
        try_rewrite_method_call(field_node->field.base);
        infer_generic(field_node->field.base, NULL);
    }
    Type* bt = Type_Infer(field_node->field.base);
    Type* st = bt;
    if (st && st->cls == TYPE_POINTER && st->pointer_base &&
        st->pointer_base->cls == TYPE_STRUCT)
        st = st->pointer_base;
    if (!st || st->cls != TYPE_STRUCT) return;
    StructDef* sd = Struct_Find(st->struct_name);
    StructField* f = sd ? Struct_FindField(sd, field_node->field.field_name,
                                            field_node->field.field_name_len) : NULL;
    if (f) return; // real field — leave to normal AST_FIELD resolution

    size_t manglen = 0;
    char* mangled = Method_Mangle(st, field_node->field.field_name, field_node->field.field_name_len, &manglen);
    if (!mangled) return; // st isn't struct-shaped enough for Method_Mangle -- same bail as before
    Symbol* msym = SymTable_Find(Get_SymTable(), mangled, manglen);
    if (!msym || msym->kind != SYM_FUNCTION) { free(mangled); return; }

    size_t old_argc = node->call.arg_count;
    ASTNode** new_args = (ASTNode**)malloc((old_argc + 1) * sizeof(ASTNode*));
    ASTNode* self_arg;
    if (bt && bt->cls == TYPE_POINTER) {
        self_arg = field_node->field.base;
    } else {
        self_arg = (ASTNode*)calloc(1, sizeof(ASTNode));
        self_arg->type = AST_ADDR;
        self_arg->unary = field_node->field.base;
    }
    new_args[0] = self_arg;
    for (size_t i = 0; i < old_argc; i++) new_args[i + 1] = node->call.args[i];
    node->call.args = new_args;
    node->call.arg_count = old_argc + 1;
    node->call.target_expr = NULL;
    node->call.target_name = mangled;
    node->call.target_name_len = manglen;
    node->call.sym = msym;
    if (sd && sd->generic_base && sd->type_arg_count > 0 && node->call.type_arg_count == 0) {
        // The struct fixes only a PREFIX of the method's type params (its own T,...);
        // the method may declare additional ones of its own (fn map[U](...)). Stash
        // the known prefix separately so infer_generic can seed from it and still
        // infer any extra trailing params from the call's arguments/return context.
        node->call.self_type_args = sd->type_args;
        node->call.self_type_arg_count = sd->type_arg_count;
    }
}

static const char* s_op_methods[] = {
    [AST_ADD] = "__add", [AST_SUB] = "__sub", [AST_MUL] = "__mul",
    [AST_DIV] = "__div", [AST_MOD] = "__mod",
    [AST_BIT_AND] = "__bitand", [AST_BIT_OR] = "__bitor", [AST_BIT_XOR] = "__bitxor",
    [AST_SHL] = "__shl", [AST_SHR] = "__shr",
    [AST_EQ] = "__eq", [AST_NEQ] = "__neq", [AST_LT] = "__lt",
    [AST_GT] = "__gt", [AST_LTE] = "__lte", [AST_GTE] = "__gte",
    [AST_ASSIGN] = "__assign",
};

// Separate (not folded into s_op_methods) because these are genuinely UNARY --
// AST_LOGICAL_NOT/AST_BIT_NOT store their one operand in node->unary, not
// node->binary.{left,right} -- and mixing node kinds with different payload
// shapes into one indexed-by-node-type table would make the array's implicit
// "covers every type from 0 up to here" sizing fragile. Unary minus (-x) is
// NOT here: the parser desugars it to `0 - x` (AST_SUB) before it ever reaches
// this table, so a struct-__neg hook would need a real AST_NEG node first --
// out of scope for now (see docs discussion), unlike ! and ~ which already are
// distinct unary AST nodes with nothing standing in the way.
static const char* s_unary_op_methods[] = {
    [AST_LOGICAL_NOT] = "__not", [AST_BIT_NOT] = "__bitnot",
};

// Shared core: rewrite `node` IN PLACE from a one- or two-operand shape
// (`recv op arg` -- binary op; `recv[arg]` indexing; or `op recv` unary, with
// arg == NULL) into `recv.mname(arg)` / `recv.mname()` (an AST_CALL over an
// AST_FIELD), PROVIDED `recv`'s type defines method `mname`. Pure AST surgery
// only -- does NOT call try_rewrite_method_call/infer_generic itself, so it's
// safe to call from a context that hasn't type-inferred this node yet
// (infer_generic's own bootstrap, below) as well as from Type_Infer (which
// immediately re-enters itself after rewriting). One core so every operator
// shape (arithmetic/comparison via s_op_methods, __index, unary via
// s_unary_op_methods, and any future one) shares the exact same rewrite
// instead of hand-copying it per operator.
static bool rewrite_operand_to_method_call(ASTNode* node, ASTNode* recv, ASTNode* arg, const char* mname) {
    // A bare type operand (`T == i32`, a generic type-param used in reflect
    // comparison position) must NOT reach Type_Infer here: it now hard-errors
    // on AST_TYPE_EXPR (see that case), and callers of this function run on
    // every candidate node unconditionally, before Typecheck_Tree's OWN
    // dedicated "cannot use a bare type as an operand" guard has had a chance
    // to fire with its more specific message. Bail quietly and let that guard
    // do its job, exactly as if this function didn't exist for this node.
    if (recv->type == AST_TYPE_EXPR || (arg && arg->type == AST_TYPE_EXPR)) return false;
    Type* rt = Type_Infer(recv);
    if (!rt) return false;
    Symbol* msym = Method_Resolve(rt, mname, strlen(mname));
    if (!msym) return false;

    // AST_ASSIGN is the one operator with a competing, ALSO-valid non-overload
    // meaning: ordinary same-type struct assignment (`b = a`). Every other
    // operator this core handles has no such competitor (Method_Resolve alone
    // is a correct-enough check there), but for `=` specifically, a struct
    // that declares __assign(i32) must NOT hijack `b = a` (a Vector assigned
    // another Vector) just because the method NAME exists -- it has to also
    // check the RHS actually fits __assign's declared parameter, via
    // check_assignable_ne (the SAME real rule check_assignable uses, not a
    // second copy of it). This has to live HERE, in the shared core, not in
    // just one caller (e.g. only Typecheck_Tree's AST_ASSIGN case) -- Type_Infer
    // and infer_generic both call this function too, unconditionally, on every
    // node they visit, and a gate bolted onto only one of the three callers
    // left the other two free to rewrite (and corrupt) an ordinary assignment
    // whenever THEY happened to visit the node first.
    if (node->type == AST_ASSIGN) {
        if (!msym->type || msym->type->cls != TYPE_FUNCTION ||
            msym->type->function.param_count < 2) {
            return false;
        }
        Type* param_t = msym->type->function.param_types[1]; // [0] is injected self
        // For a generic struct (Box[T]), Method_Resolve finds the TEMPLATE's
        // __assign, whose declared param is still the abstract T, not the
        // receiver's concrete i32 -- check_assignable_ne(T, 42, ...) would
        // then always fail (T isn't structurally equal to anything a real
        // value could be), rejecting a legitimate `b = 42` on a Box[i32].
        // Substitute using the receiver's own type_args, the same generic_base
        // + type_args pair try_rewrite_method_call uses (self_type_args) for
        // the identical purpose one level up (the CALL's return type instead
        // of this ASSIGN's argument type).
        if (rt->cls == TYPE_STRUCT) {
            param_t = Type_Substitute_Through_Instance(param_t, Struct_Find(rt->struct_name));
        }
        // A bare brace literal (`v = {1,2,3}` or `v = {.x=1,.y=2}`) is ambiguous
        // BEFORE anything resolves it: it could be ordinary construction of the
        // struct's OWN shape (ALWAYS legal, __assign or not -- see
        // assign_operator_ordinary_still_works.t) or an argument meant for
        // __assign's declared parameter (the whole point of __assign existing).
        // Must disambiguate BEFORE resolving against either target, since
        // resolve_brace_literal mutates the node in place and there's no
        // "try, then undo" once that runs.
        //  - Designated (`{.field=..}`, already AST_STRUCT_LITERAL at parse
        //    time): if every named field genuinely exists on rt, it's
        //    construction -- __assign does not apply, full stop.
        //  - Positional (`{a,b,..}`, still AST_ARRAY_LITERAL -- the parser
        //    doesn't know yet whether a bare `{...}` is a struct or array):
        //    if rt is a struct and the value count is <= rt's own field
        //    count, treat it as construction too (the same rule
        //    resolve_brace_literal's own positional-struct-literal branch
        //    uses to accept/reject a positional literal against a struct).
        // Only past both of those does this literal get a chance to mean
        // "argument to __assign" -- resolved against __assign's parameter
        // type instead of rt's own shape.
        if (rt->cls == TYPE_STRUCT) {
            StructDef* rt_sd_for_check = Struct_Find(rt->struct_name);
            if (rt_sd_for_check) {
                if (arg->type == AST_STRUCT_LITERAL && !arg->struct_lit.sdef) {
                    bool all_fields_exist = true;
                    for (size_t i = 0; i < arg->struct_lit.count; i++) {
                        if (!Struct_FindField(rt_sd_for_check, arg->struct_lit.field_names[i],
                                              arg->struct_lit.field_name_lens[i])) {
                            all_fields_exist = false;
                            break;
                        }
                    }
                    if (all_fields_exist && arg->struct_lit.count > 0) return false;
                } else if (arg->type == AST_ARRAY_LITERAL && !arg->array_lit.elem_type &&
                           arg->array_lit.count <= rt_sd_for_check->field_count) {
                    return false;
                }
            }
        }
        resolve_brace_literal(arg, param_t);
        bool matches = false;
        if (msym->generic_decl && msym->generic_decl->func_decl.type_param_count > 0) {
            ASTNode* func = msym->generic_decl;
            size_t pc = func->func_decl.type_param_count;
            Type** inferred = (Type**)calloc(pc, sizeof(Type*));
            Type* arg_t = Type_Infer(arg);
            matches = unify_types(arg_t, param_t, func->func_decl.type_params, inferred, pc);
            free(inferred);
        } else {
            matches = check_assignable_ne(param_t, arg, "__assign argument");
        }
        if (!matches) return false;
    }

    ASTNode* target = (ASTNode*)calloc(1, sizeof(ASTNode));
    target->type = AST_FIELD;
    target->field.base = recv;
    target->field.field_name = mname;
    target->field.field_name_len = strlen(mname);
    target->line = node->line;
    target->column = node->column;

    node->type = AST_CALL;
    node->call.target_expr = target;
    if (arg) {
        ASTNode** args = (ASTNode**)calloc(1, sizeof(ASTNode*));
        args[0] = arg;
        node->call.args = args;
        node->call.arg_count = 1;
    } else {
        node->call.args = NULL;
        node->call.arg_count = 0;
    }
    return true;
}

// s_op_methods case (a+b -> a.__add(b), etc.): needed at BOTH Type_Infer
// (lazy) and infer_generic (top-down, before Type_Infer ever runs on this
// node) -- see infer_generic's call site for why a generic struct's
// overloaded operator (Box[T]'s __add) needs the earlier hook too, not just
// the lazy one Type_Infer would otherwise rely on alone.
// Not static: ConstEval (constexpr.c) needs this too -- see its own binary-op
// case for why. Same rewrite, same one implementation, used by Type_Infer,
// infer_generic, AND ConstEval now instead of ConstEval growing a second,
// independent "does this operator have an overload" check of its own.
bool try_rewrite_operator_method(ASTNode* node) {
    if (node->type >= (ASTNodeType)(sizeof(s_op_methods)/sizeof(s_op_methods[0]))) return false;
    const char* mname = s_op_methods[node->type];
    if (!mname) return false;
    return rewrite_operand_to_method_call(node, node->binary.left, node->binary.right, mname);
}

// v[i] -> v.__index(i), same shared core, same two call sites (Type_Infer +
// infer_generic) and same reason: infer_generic must see this already
// AST_CALL-shaped BEFORE Type_Infer's lazy rewrite would otherwise fire, or a
// generic struct's __index return type never gets its self_type_args
// substituted (identical failure mode s_op_methods had for Box[T]'s __add).
// Not static -- ConstEval needs this too, same reason as
// try_rewrite_operator_method (see that function's own comment).
bool try_rewrite_index_method(ASTNode* node) {
    if (node->type != AST_INDEX) return false;
    return rewrite_operand_to_method_call(node, node->index.base, node->index.index, "__index");
}

// If __index returns T* (a pointer INTO the container, e.g. &self.data[i]),
// v[i] must mean "the T at that address," not "the address itself" -- same
// read/write duality a raw array/pointer index already has (specs.md: "array
// indexing auto-derefs through a pointer"). That existing auto-deref is
// written specifically for AST_INDEX/AST_FIELD whose BASE is a pointer; it
// has no idea an AST_CALL's return value might also want the same treatment,
// because nothing needed that before __index existed. Rather than teach
// AST_CALL a bespoke special case, reuse the ACTUAL general mechanism this
// language already has for "dereference a pointer value": AST_DEREF, whose
// own Type_Infer case already handles any pointer operand uniformly, and
// whose compile_lvalue case already treats the operand's value as the write
// address -- both for free, no new logic in either.
//
// MUST run strictly AFTER try_rewrite_index_method's own follow-up
// (try_rewrite_method_call + infer_generic, done by whichever caller invoked
// the rewrite) -- those two expect an AST_CALL shape (node->call.*) and read
// garbage/wrong union fields if `node` has already become AST_DEREF by the
// time they run. So this is a SEPARATE step, applied once after the call is
// fully resolved, not folded into try_rewrite_index_method itself.
static void wrap_index_result_deref(ASTNode* node) {
    if (node->type != AST_CALL || node->call.index_deref_wrapped) return;
    // try_rewrite_method_call (already run by the caller before this) resolves
    // target_expr's AST_FIELD to a mangled direct-call symbol and clears
    // target_expr to NULL (see its own doc comment) -- so by the time this
    // runs, the method name can only be read off node->call.sym, not
    // target_expr->field.field_name anymore.
    Symbol* sym = node->call.sym;
    // Mangled name is "StructName_" + method name, e.g. "Vector_[i32]___index"
    // for a generic instantiation -- match the SUFFIX, not an exact/prefix
    // compare, since the struct-name prefix varies per instantiation.
    if (!sym || sym->name_len < 7 ||
        strncmp(sym->name + sym->name_len - 7, "__index", 7) != 0) {
        return;
    }
    Type* rt = Type_Infer(node);
    if (!rt || rt->cls != TYPE_POINTER) return;
    // `node` is a live pointer other tree parents already hold -- move its
    // contents into a fresh node and turn `node` itself into the AST_DEREF
    // wrapper, so every existing reference to `node` sees the deref.
    ASTNode* call_copy = (ASTNode*)malloc(sizeof(ASTNode));
    *call_copy = *node;
    // A later walk (e.g. the AST_DEREF wrapper's own Typecheck_Tree(node->unary)
    // recursing into call_copy) will re-enter infer_generic/Type_Infer on THIS
    // node -- without this flag, that visit's mangled-name check ("does this
    // resolve to an __index method?") still matches, and it would get wrapped
    // in a SECOND AST_DEREF (deref of a deref of the actual T*), producing
    // "cannot dereference non-pointer type 'T'" once the first deref already
    // reduced it to a plain T.
    call_copy->call.index_deref_wrapped = true;
    node->type = AST_DEREF;
    node->result_type = NULL; // stale T* cache lives on call_copy now; re-infer for the deref
    node->unary = call_copy;
}

// !v -> v.__not(), ~v -> v.__bitnot(): same shared core (with arg == NULL, a
// zero-arg method call), same two call sites and reason as the two above.
// Unary minus (-x) is NOT handled here -- see s_unary_op_methods' own comment.
// Not static -- ConstEval needs this too, same reason as
// try_rewrite_operator_method (see that function's own comment).
bool try_rewrite_unary_operator_method(ASTNode* node) {
    if (node->type >= (ASTNodeType)(sizeof(s_unary_op_methods)/sizeof(s_unary_op_methods[0]))) return false;
    const char* mname = s_unary_op_methods[node->type];
    if (!mname) return false;
    return rewrite_operand_to_method_call(node, node->unary, NULL, mname);
}

// (T)x -> x.__cast[T]() -- the cast-operator overload. Doesn't fit
// rewrite_operand_to_method_call's shape (a VALUE argument, arity 0 or 1):
// __cast takes no value argument at all, but one explicit TYPE argument (the
// cast's own target_type), attached the same way an ordinary explicit
// generic call (`identity[i32](x)`) already carries node->call.type_args --
// reuses that exact field/mechanism rather than inventing a second way to
// pass a type into a call. Not static -- ConstEval needs this too, same
// reason as the other three rewrites.
bool try_rewrite_cast_operator(ASTNode* node) {
    if (node->type != AST_CAST) return false;
    if (node->cast.expr->type == AST_TYPE_EXPR) return false; // see check_assignable's own guard
    Type* rt = Type_Infer(node->cast.expr);
    if (!rt) return false;
    Symbol* msym = Method_Resolve(rt, "__cast", 6);
    if (!msym) return false;
    // A NON-generic __cast() only knows how to produce ONE fixed type (its
    // declared return type) -- attaching an explicit type arg to a call that
    // has no type parameter to substitute it into is silently a no-op, which
    // would let `(i32)m` on a `fn __cast() f64` wrongly "dispatch" and return
    // f64 bits reinterpreted as i32 instead of erroring or falling back. Only
    // apply __cast here when it's either generic (the type arg has somewhere
    // to go) or its fixed return type already matches the cast's own target.
    if (!msym->generic_decl && msym->type && msym->type->cls == TYPE_FUNCTION &&
        !Type_Equals(msym->type->function.return_type, node->cast.target_type)) {
        return false;
    }

    ASTNode* target = (ASTNode*)calloc(1, sizeof(ASTNode));
    target->type = AST_FIELD;
    target->field.base = node->cast.expr;
    target->field.field_name = "__cast";
    target->field.field_name_len = 6;
    target->line = node->line;
    target->column = node->column;

    Type** targs = (Type**)malloc(sizeof(Type*));
    targs[0] = node->cast.target_type;

    node->type = AST_CALL;
    node->call.target_expr = target;
    node->call.args = NULL;
    node->call.arg_count = 0;
    node->call.type_args = targs;
    node->call.type_arg_count = 1;
    return true;
}

// Try every operator-overload rewrite in dispatch-priority order, short-
// circuiting on the first that applies. Node has at most one dunder overload
// shape (it can't simultaneously be a binary op AND a cast, say), so exactly
// one of these ever rewrites it -- the chain is just "which one, if any."
static bool try_rewrite_any_operator(ASTNode* node) {
    return try_rewrite_operator_method(node) || try_rewrite_index_method(node) ||
           try_rewrite_unary_operator_method(node) || try_rewrite_cast_operator(node);
}

Type* Type_Infer(ASTNode* node) {
    if (!node) return NULL;
    if (node->result_type) return node->result_type;

    // Struct_FindField searches DATA fields (sd->fields[]) -- an impl method is
    // never in that list (it's resolved by mangled symbol name), which used to
    // make dispatch permanently false for every struct: a+b (or a[i]) on a
    // struct silently fell through to built-in lanewise/array handling instead
    // of ever reaching __add/__index. rewrite_operand_to_method_call uses
    // Method_Resolve (the real "does this type have this method" query)
    // instead. The two follow-up calls (try_rewrite_method_call to set .sym/
    // self_type_args, infer_generic to substitute them into the return type)
    // are what a NON-generic struct doesn't need but a generic one (Box[T]'s
    // __add/__index) does -- see rewrite_operand_to_method_call's own comment.
    if (try_rewrite_any_operator(node)) {
        try_rewrite_method_call(node);
        infer_generic(node, NULL);
        wrap_index_result_deref(node);
        return Type_Infer(node);
    }

    Type* t = NULL;
    switch (node->type) {
        case AST_INT_LITERAL:
            if (node->lit_kind == LIT_BOOL) {
                t = make_prim(PRIM_BOOL);
            } else if (node->lit_kind == LIT_NULL) {
                t = make_ptr(make_prim(PRIM_V)); // null pointer literal: void* (comparable to any T*)
            } else if (node->lit_kind == LIT_FLOAT) {
                t = make_prim(PRIM_F64); // float literals default to f64
            } else {
                // Spec: i32 if it fits, i64 otherwise.
                t = (node->int_value <= 0x7fffffff) ? make_prim(PRIM_I32)
                                                    : make_prim(PRIM_I64);
            }
            break;

        case AST_IDENT:
            t = node->ident.sym ? node->ident.sym->type : NULL;
            if (!t && s_ce_generic_params) {
                for (size_t i = 0; i < s_ce_generic_n; i++) {
                    if (strlen(s_ce_generic_params[i]) == node->ident.name_len &&
                        strncmp(node->ident.name, s_ce_generic_params[i], node->ident.name_len) == 0) {
                        Type* cv = s_ce_generic_args[i];
                        if (cv && cv->cls == TYPE_CONST_VALUE) t = cv->cval.pin;
                        else t = cv;
                        break;
                    }
                }
            }
            // If this ident was resolved as a monomorphized generic (value position),
            // return the concrete substituted fn type so check_assignable sees it correctly.
            if (t && node->ident.type_arg_count > 0 && node->ident.sym->generic_decl) {
                ASTNode* gdecl = node->ident.sym->generic_decl;
                t = Type_Substitute(t, gdecl->func_decl.type_params,
                                    node->ident.type_args, node->ident.type_arg_count);
            }
            // A bare reference to a concrete (non-generic) top-level function --
            // `asc` in `apply(3, 5, asc)` -- carries more than its calling
            // shape: it names a SPECIFIC function. Wrap it as TYPE_FN_LITERAL so
            // that binding a generic parameter to it (unify_types, the TYPE_PARAM
            // branch) records WHICH function, not just the shared shape -- this
            // is what makes `apply(x,y,asc)` and `apply(x,y,desc)` two distinct
            // instantiations instead of one shared indirect-call body, and what
            // lets the call-codegen direct-call shortcut above find a concrete
            // Symbol* to call directly. Every other consumer of this ident's type
            // (assigning it to a `fn(...) T` variable, taking its address, an
            // ordinary direct `asc(3,5)` call which never reaches this case
            // anyway) goes through Type_FnLitShape at its own check site and
            // sees the plain TYPE_FUNCTION underneath, so nothing about how a
            // function name behaves elsewhere changes.
            //
            // Excluded on purpose: generic functions (no single Symbol* --
            // `identity` alone doesn't name one concrete function), extern
            // functions (identity isn't the point for a C ABI symbol; the
            // existing address-resolution path is what those need), and a
            // reference already carrying explicit [T] type args (handled above
            // via generic_decl, mutually exclusive with being non-generic here).
            if (t && t->cls == TYPE_FUNCTION && node->ident.sym &&
                node->ident.sym->kind == SYM_FUNCTION &&
                !node->ident.sym->generic_decl && !node->ident.sym->is_extern) {
                Type* lit = (Type*)calloc(1, sizeof(Type));
                lit->cls = TYPE_FN_LITERAL;
                lit->fn_lit.sym = node->ident.sym;
                lit->fn_lit.sig = t;
                t = lit;
            }
            break;
            break;

        case AST_CALL:
            try_rewrite_method_call(node);
            if (node->call.target_expr) {
                Type* tgt = fn_lit_shape(Type_Infer(node->call.target_expr));
                if (tgt && tgt->cls == TYPE_FUNCTION) t = tgt->function.return_type;
                else if (tgt && tgt->cls == TYPE_POINTER && tgt->pointer_base && tgt->pointer_base->cls == TYPE_FUNCTION)
                    t = tgt->pointer_base->function.return_type;
                else if (try_rewrite_call_operator(node, tgt)) {
                    try_rewrite_method_call(node);
                    return Type_Infer(node);
                }
                else { Error_AtNode(node, "calling non-function", NULL); }
            } else {
                Type* tgt = node->call.sym ? node->call.sym->type : NULL;
                if (tgt && tgt->cls == TYPE_FUNCTION) t = tgt->function.return_type;
                
                // If this is a generic call, substitute the generic return type with concrete types
                if (node->call.sym && node->call.sym->generic_decl && 
                    node->call.sym->generic_decl->func_decl.type_param_count > 0 && 
                    node->call.type_arg_count > 0) {
                    t = Type_Substitute(t, node->call.sym->generic_decl->func_decl.type_params, 
                                        node->call.type_args, node->call.type_arg_count);
                }
            }
            break;

        case AST_DEREF: {
            Type* base = Type_Infer(node->unary);
            t = (base && base->cls == TYPE_POINTER) ? base->pointer_base : NULL;
            break;
        }

        case AST_ADDR:
            t = make_ptr(Type_Infer(node->unary));
            break;

        case AST_CAST:
            t = node->cast.target_type; // a cast's result type is exactly the target
            break;

        case AST_FIELD: {
            // base may be a struct value or a pointer to struct (one-level auto-deref).
            Type* bt = Type_Infer(node->field.base);
            StructDef* sd = NULL;
            if (bt && bt->cls == TYPE_STRUCT) {
                sd = Struct_Find(bt->struct_name);
            } else if (bt && bt->cls == TYPE_POINTER &&
                       bt->pointer_base && bt->pointer_base->cls == TYPE_STRUCT) {
                sd = Struct_Find(bt->pointer_base->struct_name); // auto-deref one level
            } else if (bt && bt->cls == TYPE_POINTER &&
                       bt->pointer_base && bt->pointer_base->cls == TYPE_POINTER) {
                Error_AtNode(node, "field access on pointer-to-pointer; use (*pp).field", NULL);
            }
            if (!sd) {
                char msg[160];
                snprintf(msg, sizeof(msg), "field access '.%.*s' on a non-struct value",
                         (int)node->field.field_name_len, node->field.field_name);
                Error_AtNode(node, msg, NULL);
            }
            StructField* f = Struct_FindField(sd, node->field.field_name, node->field.field_name_len);
            if (!f) {
                char msg[192];
                snprintf(msg, sizeof(msg), "%s '%s' has no field '%.*s'", sd->is_overlapping ? "union" : "struct", sd->name,
                         (int)node->field.field_name_len, node->field.field_name);
                Error_AtNode(node, msg, NULL);
            }
            node->field.field = f;
            node->field.sdef = sd;
            Struct_Layout(sd); // ensure field offsets are computed before codegen reads them
            t = f->type;
            break;
        }

        case AST_STRUCT_LITERAL: {
            // An unresolved literal (sdef==NULL) has no concrete type *yet*. This is
            // normal during inference probing (e.g. the bottom-up arg sweep in
            // infer_generic_call): return NULL and let the caller fall back to
            // top-down resolution, exactly as an unresolved array literal does. A
            // literal that is STILL unresolved when its value is actually needed is
            // caught at codegen (emit) with a clean diagnostic — not here, so we
            // don't kill cases that resolve top-down a moment later.
            if (!node->struct_lit.sdef) { t = NULL; break; }
            t = (Type*)calloc(1, sizeof(Type));
            t->cls = TYPE_STRUCT;
            t->struct_name = node->struct_lit.sdef->name;
            break;
        }

        case AST_INDEX: {
            Type* base = Type_Infer(node->index.base);
            if (base && base->cls == TYPE_ARRAY) t = base->array.element;
            else if (base && base->cls == TYPE_POINTER) {
                // "array indexing auto-derefs through a pointer" (specs.md §8):
                // p[i] means (*p)[i]. When the pointee is itself an array type
                // (e.g. p: u32[8]*), the element we want is the pointee
                // ARRAY's element type, not the pointee type itself -- else
                // p[i] would wrongly infer as u32[8] instead of u32.
                Type* pointee = base->pointer_base;
                t = (pointee && pointee->cls == TYPE_ARRAY) ? pointee->array.element : pointee;
            }
            else { Error_AtNode(node, "indexing a non-array, non-pointer", NULL); }
            break;
        }

        case AST_ARRAY_LITERAL:
            t = node->result_type; // set at parse time (full array type w/ resolved size)
            break;

        case AST_SIZEOF:
        case AST_ALIGNOF:
        case AST_OFFSETOF:
            t = make_prim(PRIM_U64); // sizeof/alignof/offsetof all yield u64 (spec)
            break;

        case AST_NAMEOF:
            // Should always have folded into a real AST_STRING before reaching
            // here (parse time, or clone_ast at generic instantiation) -- this
            // exists only as a defensive fallback so an unresolved nameof gets
            // a sane type instead of falling through to the "unknown node" path.
            t = make_ptr(make_prim(PRIM_U8));
            break;

        case AST_STRING:
            t = make_ptr(make_prim(PRIM_U8)); // u8* to static bytes
            break;

        case AST_NEW:
            // new T (single or [n]) yields T* in both cases.
            t = make_ptr(node->new_expr.alloc_type);
            break;

        case AST_ASSIGN:
            t = Type_Infer(node->binary.left);
            break;

        // Comparisons and logical ops yield bool.
        case AST_EQ: case AST_NEQ: case AST_LT:
        case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_LOGICAL_NOT:
            t = make_prim(PRIM_BOOL);
            break;

        case AST_BIT_NOT:
            t = Type_Infer(node->unary);
            break;

        // Shifts take the type of the left operand.
        case AST_SHL: case AST_SHR:
            t = Type_Infer(node->binary.left);
            break;

        // Arithmetic / bitwise: usual-arithmetic-conversion-lite.
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
            t = arith_result(Type_Infer(node->binary.left),
                             Type_Infer(node->binary.right),
                             node->binary.left, node->binary.right);
            break;

        case AST_IF: {
            Type* t1 = Type_Infer(node->if_stmt.true_block);
            Type* t2 = node->if_stmt.false_block ? Type_Infer(node->if_stmt.false_block) : NULL;
            t = t1 ? t1 : t2; // simplified type unification
            break;
        }

        case AST_BLOCK: {
            if (node->block.count > 0) {
                t = Type_Infer(node->block.statements[node->block.count - 1]);
            }
            break;
        }

        // A bare type used in value position (`void`, `i32`, a struct/enum name,
        // a generic type-param) parses to AST_TYPE_EXPR. It has no case above
        // because it has NO VALUE -- it can't be loaded, stored, added, compared,
        // or put in a field/element/argument. Every LEGITIMATE consumer (T == i32
        // and match T { ... } in Typecheck_Tree/parser.c, generic substitution in
        // clone_ast, const-generic folding in ConstEval) checks node->type ==
        // AST_TYPE_EXPR directly and branches away BEFORE ever calling Type_Infer
        // on it -- none of them depend on this function returning NULL for it.
        // Every OTHER call site, though, treats Type_Infer's NULL as "no type,
        // handle as void/absent" -- which is wrong here: this NULL doesn't mean
        // absent, it means "this node should never have reached here as a value."
        // That conflation is what let a bare type slip through as a value again
        // and again (declaration init, struct field, array element, unary
        // operand, ... — the same root cause documented repeatedly in
        // KNOWN_BUGS.md), because each caller had to remember to guard for
        // AST_TYPE_EXPR itself instead of trusting Type_Infer's result. Erroring
        // here, once, at the single function every value-producing path already
        // calls, closes the whole class instead of chasing one call site at a time.
        case AST_TYPE_EXPR:
            Error_AtNode(node, "a type cannot be used as a value here", NULL);
            return NULL; // unreached (Error_AtNode exits), quiets -Wreturn-type

        default:
            t = NULL;
            break;
    }

    return t;
}

// The enclosing function's return type, so a `return {...}` bare literal can be
// resolved against it. Set when entering a function body, restored on exit.
static Type* s_current_fn_return = NULL;

// The type params in scope where the expression currently being type-inferred was
// WRITTEN -- set by the parser around the eager parse-time typecheck of an
// `auto`/`unpack` initializer inside a generic body (parse_unpack). It lets
// unify_types tell "this arg is one of my enclosing generic's OWN params" (e.g.
// `inner(v)` where v:T and T is outer_diff's own param -> bind U:=T, yielding the
// inner instantiation inner[T], re-substituted concrete at monomorphization) apart
// from "this arg is some other call's still-unresolved return param" (`make()` in
// `wrap(make())`, which must stay unbound so top-down inference from the outer
// Box[i32] context fills it in). Only the former is a genuine bound param in scope.
static const char** s_encl_type_params = NULL;
static size_t s_encl_type_param_count = 0;
void Types_SetEnclosingParams(const char** params, size_t count) {
    s_encl_type_params = params; s_encl_type_param_count = count;
}
static bool is_enclosing_param(Type* t) {
    if (!t || t->cls != TYPE_PARAM || !t->param_name) return false;
    for (size_t i = 0; i < s_encl_type_param_count; i++)
        if (s_encl_type_params[i] && strcmp(s_encl_type_params[i], t->param_name) == 0)
            return true;
    return false;
}

// Callee param types for the call currently having its args typechecked, so a bare
// `{...}` passed as an argument resolves against the matching parameter type.


// Unifies a concrete type against a generic template type to infer type parameters.
static bool unify_types(Type* concrete, Type* generic, const char** type_params, Type** inferred_args, size_t param_count) {
    if (!concrete || !generic) return concrete == generic;
    // A TYPE_PARAM on the concrete side means the argument is itself an unresolved
    // generic (e.g. the return type of an inner generic call whose T hasn't been
    // inferred yet). Binding from an unresolved param would poison the table with a
    // non-concrete type; skip and let a later top-down or outer pass fill it in.
    //
    // Exception: when the concrete param is one of the ENCLOSING generic's OWN
    // params (a real bound param in scope -- `inner(v)` with v:T inside
    // `outer_diff[T]`) AND the callee's param is a bare TYPE_PARAM, binding it is
    // correct: it produces the inner instantiation `inner[T]`, and clone_ast
    // re-substitutes T to a concrete type when the enclosing generic is
    // monomorphized. This is what makes `auto x = inner(v)` (which typechecks its
    // initializer eagerly at parse time, while T is still abstract) work. A param
    // from some OTHER unresolved call (`make()` in `wrap(make())`) is NOT enclosing,
    // so it still bails here and waits for top-down inference. See s_encl_type_params.
    if (concrete->cls == TYPE_PARAM &&
        !(generic->cls == TYPE_PARAM && is_enclosing_param(concrete)))
        return false;
    // A TYPE_FN_LITERAL binding DIRECTLY to a bare type param (`Cmp cmp`, generic
    // side is TYPE_PARAM) is the one case that should keep the literal's identity
    // -- that's the entire feature (see the TYPE_PARAM branch below, which stores
    // `concrete` as-is). Everywhere else the literal is reached through STRUCTURAL
    // recursion (`fn(T) T f` matching against a concrete `fn(u32) u32`-shaped
    // literal, where only the inner `T` is what's being solved for) -- there,
    // only the calling-convention shape is relevant, and the literal's TYPE_FUNCTION
    // class must show through or every structural case below (which requires
    // `generic->cls == concrete->cls`) rejects a perfectly good match. Unwrap here,
    // once, rather than re-deriving "is this the direct-bind case" at every
    // individual structural branch.
    if (concrete->cls == TYPE_FN_LITERAL && generic->cls != TYPE_PARAM) {
        concrete = Type_FnLitShape(concrete);
    }
    // A deferred const-value whose expr is a bare param ident (the `N` in a generic
    // return type `W[T, N]`) binds exactly like a TYPE_PARAM: match it against the
    // concrete value the argument supplies. This is what lets `take(mk())` recover
    // N from take`s param type `W[i32, 5]`.
    if (generic->cls == TYPE_CONST_VALUE && generic->cval.defer &&
        generic->cval.defer->type == AST_IDENT) {
        ASTNode* id = generic->cval.defer;
        size_t idx = 0;
        for (; idx < param_count; idx++)
            if (strlen(type_params[idx]) == id->ident.name_len &&
                strncmp(type_params[idx], id->ident.name, id->ident.name_len) == 0) break;
        if (idx == param_count) return false;
        // concrete side is the value the arg pins (a TYPE_CONST_VALUE, or an array
        // count lifted into one). Normalize to a concrete const-value.
        Type* cval = concrete;
        if (concrete->cls != TYPE_CONST_VALUE) return false;
        if (inferred_args[idx]) return Type_Equals(inferred_args[idx], cval);
        inferred_args[idx] = cval;
        return true;
    }
    if (generic->cls == TYPE_PARAM) {
        size_t idx = 0;
        for (; idx < param_count; idx++) {
            if (strcmp(type_params[idx], generic->param_name) == 0) break;
        }
        if (idx == param_count) return false; // unknown type param
        if (inferred_args[idx]) {
            return Type_Equals(inferred_args[idx], concrete);
        } else {
            inferred_args[idx] = concrete;
            return true;
        }
    }
    if (generic->cls != concrete->cls) return false;
    switch (generic->cls) {
        case TYPE_POINTER: 
            return unify_types(concrete->pointer_base, generic->pointer_base, type_params, inferred_args, param_count);
        case TYPE_ARRAY: {
            bool sizes_match = (concrete->array.count == generic->array.count);
            ASTNode* ce = generic->array.count_expr;
            // A bare generic-param array size can show up as either an
            // AST_IDENT (parsed outside expression context, e.g. directly
            // inside array-dimension brackets) or AST_TYPE_EXPR (a bare type
            // param now also recognized as a general expression-position
            // primary) — same underlying "which in-scope param is this"
            // check either way, just reading the name from a different field.
            const char* pname = NULL; size_t plen = 0;
            if (ce && ce->type == AST_IDENT) { pname = ce->ident.name; plen = ce->ident.name_len; }
            else if (ce && ce->type == AST_TYPE_EXPR && ce->sizeof_expr.type &&
                     ce->sizeof_expr.type->cls == TYPE_PARAM) {
                pname = ce->sizeof_expr.type->param_name; plen = pname ? strlen(pname) : 0;
            }
            if (!sizes_match && pname) {
                size_t idx = 0;
                for (; idx < param_count; idx++) {
                    if (strlen(type_params[idx]) == plen &&
                        strncmp(type_params[idx], pname, plen) == 0) {
                        break;
                    }
                }
                if (idx < param_count) {
                    Type* cval = (Type*)calloc(1, sizeof(Type));
                    cval->cls = TYPE_CONST_VALUE;
                    cval->const_val = concrete->array.count;
                    if (inferred_args[idx]) {
                        if (!Type_Equals(inferred_args[idx], cval)) {
                            free(cval);
                            return false;
                        }
                        free(cval);
                        sizes_match = true;
                    } else {
                        inferred_args[idx] = cval;
                        sizes_match = true;
                    }
                }
            }
            return sizes_match && 
                   unify_types(concrete->array.element, generic->array.element, type_params, inferred_args, param_count);
        }
        case TYPE_STRUCT:
            if (generic->struct_name && concrete->struct_name) {
                StructDef* gs = Struct_Find(generic->struct_name);
                StructDef* cs = Struct_Find(concrete->struct_name);
                if (gs && cs && gs->generic_base && cs->generic_base && gs->generic_base == cs->generic_base) {
                    for (size_t i = 0; i < gs->type_arg_count; i++) {
                        if (!unify_types(cs->type_args[i], gs->type_args[i], type_params, inferred_args, param_count)) return false;
                    }
                    return true;
                }
                return strcmp(generic->struct_name, concrete->struct_name) == 0;
            }
            return false;
        case TYPE_FUNCTION:
            if (generic->function.param_count != concrete->function.param_count) return false;
            if (generic->function.is_vararg != concrete->function.is_vararg) return false;
            if (!unify_types(concrete->function.return_type, generic->function.return_type, type_params, inferred_args, param_count)) return false;
            for (size_t i = 0; i < generic->function.param_count; i++) {
                if (!unify_types(concrete->function.param_types[i], generic->function.param_types[i], type_params, inferred_args, param_count)) return false;
            }
            return true;
        case TYPE_FN_LITERAL:
            // Both sides already-resolved literals reaching this case (rather
            // than the TYPE_PARAM branch above binding one of them) means both
            // are concrete: two literals unify only if they name the same
            // function. Nominal, matching Type_Equals.
            return generic->fn_lit.sym == concrete->fn_lit.sym;
        case TYPE_PRIMITIVE:
            return generic->primitive == concrete->primitive;
        default:
            return false;
    }
}

// Attempts to infer missing generic type arguments for an AST_CALL.
static bool is_untyped_int_literal(ASTNode* n);
static bool is_untyped_float_literal(ASTNode* n);
static bool is_null_literal(ASTNode* n);
static bool is_untyped_literal(ASTNode* n);

// Attempts to infer missing generic type arguments for an unresolved generic
// reference -- either a CALL to a generic function, or a bare IDENT naming one
// in value position (e.g. `fn(u32) u32 f = identity`, no call parens). These
// used to be two separately-written functions (infer_generic_call,
// resolve_generic_ident_to_fnptr) that did the same unify/calloc/writeback
// dance for two parallel sets of AST fields (call.{sym,type_args,...} vs
// ident.{sym,type_args,...}); merged into one dispatch so there's a single
// place implementing "push a target type into an unresolved generic
// reference" rather than two copies that every call site had to remember to
// invoke together.
//
// The two node kinds are NOT behaviorally identical, and this preserves that
// on purpose rather than flattening it:
//   - AST_CALL has arguments to learn from, so after the (optional) top-down
//     unify against `target`, it also does a bottom-up pass over the actual
//     call arguments. AST_IDENT in value position has no arguments at all --
//     it's a bare reference -- so only the top-down step applies, and `target`
//     must be a concrete TYPE_FUNCTION or there's nothing to unify against.
//   - AST_CALL's top-down step ignores a `false` from unify_types and only
//     checks at the end whether every param slot got filled (deferred
//     mismatch is generally caught downstream once T is concrete, by the
//     ordinary argument-type check). AST_IDENT's single top-down step bails
//     immediately on a `false` from unify_types, since there's no downstream
//     argument check that would otherwise catch a real conflict for a bare
//     value-position reference. Kept exactly as each originally behaved.
// Prototype: `T... args` pack expansion. Bundle every trailing call argument
// into ONE synthesized anon-struct value at this call's pack slot. Must run
// before ANY unification touches this call node -- infer_generic is reachable
// from several top-down entry points (return/decl/assign targets, nested call
// args), and each clamps its own arg-sweep to the declared (pre-expansion)
// param count, so whichever entry point gets here first has to see the
// already-collapsed 1-struct-arg shape, not the original N-arg list.
static void pack_expand_call_args(ASTNode* node) {
    if (node->call.pack_rewritten) return;
    if (!node->call.sym || !node->call.sym->func_decl) return;
    int pidx = node->call.sym->func_decl->func_decl.pack_param_index;
    if (pidx < 0) return;
    if (node->call.arg_count < (size_t)pidx) return;
    size_t pack_n = node->call.arg_count - (size_t)pidx;
    Type** ftypes = (Type**)malloc((pack_n ? pack_n : 1) * sizeof(Type*));
    ASTNode** vals = (ASTNode**)malloc((pack_n ? pack_n : 1) * sizeof(ASTNode*));
    bool ok = true;
    for (size_t i = 0; i < pack_n; i++) {
        ASTNode* a = node->call.args[(size_t)pidx + i];
        Typecheck_Tree(a);
        Type* t = Type_Infer(a);
        if (!t) {
            fprintf(stderr, "Error: cannot infer type of pack argument %zu in call to '%s'\n",
                    i + 1, node->call.sym->name);
            ok = false; break;
        }
        ftypes[i] = t;
        vals[i] = a;
    }
    if (ok) {
        StructDef* sd = Struct_MakeAnon(ftypes, pack_n, false); // a T... call-arg bundle is always struct-shaped
        // field_names/field_name_lens are indexed unconditionally by later
        // typecheck passes even when sdef is pre-set (positional literal), so
        // mirror the synthesized "_0","_1",... names Struct_MakeAnon gave each
        // field -- can't leave these NULL.
        const char** fnames = (const char**)malloc((pack_n ? pack_n : 1) * sizeof(char*));
        size_t* fnlens = (size_t*)malloc((pack_n ? pack_n : 1) * sizeof(size_t));
        for (size_t i = 0; i < pack_n; i++) {
            fnames[i] = sd->fields[i].name;
            fnlens[i] = strlen(sd->fields[i].name);
        }
        ASTNode* lit = (ASTNode*)calloc(1, sizeof(ASTNode));
        lit->type = AST_STRUCT_LITERAL;
        lit->struct_lit.sdef = sd;
        lit->struct_lit.values = vals;
        lit->struct_lit.count = pack_n;
        lit->struct_lit.field_names = fnames;
        lit->struct_lit.field_name_lens = fnlens;
        lit->struct_lit.is_enum_variant = false;
        node->call.args[(size_t)pidx] = lit;
        node->call.arg_count = (size_t)pidx + 1;
    } else {
        free(ftypes); free(vals);
    }
    node->call.pack_rewritten = true;
}

void infer_generic(ASTNode* node, Type* target) {
    if (!node) return;

    // An operator over a struct with a matching dunder method (a+b where
    // Box[T] defines __add, or a[i] where it defines __index) hasn't been
    // rewritten into AST_CALL yet the FIRST time a top-down caller (e.g.
    // Typecheck_Tree's AST_DECLARATION) reaches here -- that rewrite normally
    // happens lazily inside Type_Infer, which hasn't run on this node yet. Do
    // it now so the AST_CALL branch below (and its self_type_args
    // substitution) actually sees an AST_CALL this call, instead of only on
    // some LATER visit after Type_Infer already fired.
    try_rewrite_any_operator(node);

    if (node->type == AST_CALL) {
        pack_expand_call_args(node);
        if (node->call.target_expr && node->call.target_expr->type == AST_FIELD && !node->call.sym) {
            try_rewrite_method_call(node);
        }
        // Must run BEFORE the early returns below (non-generic callee, no
        // explicit generic decl, ...) -- this is the only place a v[i] whose
        // rewrite happened HERE (not lazily inside Type_Infer) gets its
        // pointer-returning __index wrapped in the auto-deref every other
        // __index call site already gets. A non-generic Fixed.__index bails
        // at the very next line (`!generic_decl`), so tucking this after
        // those checks would silently skip the non-generic case entirely.
        wrap_index_result_deref(node);
        if (node->call.type_arg_count > 0) return; // already explicit
        if (!node->call.sym || !node->call.sym->generic_decl) return; // not generic
        if (!node->call.sym->type || node->call.sym->type->cls != TYPE_FUNCTION) return;

        ASTNode* decl = node->call.sym->generic_decl;
        size_t param_count = decl->func_decl.type_param_count;
        if (param_count == 0) return;

        Type** inferred_args = (Type**)calloc(param_count, sizeof(Type*));

        // Seed any prefix already fixed by an instantiated generic struct (impl
        // method on Box[i32]: T is known, but an extra method-level U is not).
        for (size_t i = 0; i < node->call.self_type_arg_count && i < param_count; i++)
            inferred_args[i] = node->call.self_type_args[i];

        // Bottom-up pass 1: TYPED args only — real concrete types are ground
        // truth and get first say. Untyped literals are deferred to pass 2
        // below (after target), since they can still adapt to whatever T
        // ends up being; they shouldn't get to pin T ahead of the target.
        // (KNOWN BUG #6: two disagreeing untyped literals, e.g. mixed
        // `add(3.2, 10)`, are still first-wins between each other within
        // pass 2 — that part is unchanged; see docs/KNOWN_BUGS.md.)
        size_t arg_count = node->call.sym->type->function.param_count;
        for (size_t i = 0; i < node->call.arg_count && i < arg_count; i++) {
            ASTNode* a = node->call.args[i];
            if (is_untyped_literal(a))
                continue;
            // An argument that is ITSELF an uninferred generic call (`id(id(5))`)
            // must be resolved before we can read its type: without this, Type_Infer
            // returns the callee's unsubstituted return param (a TYPE_PARAM), which
            // unify skips, so the outer call never pins its own T. Recurse first.
            // (Top-down positions -- decl init, return, call arg -- already do this
            // via their own infer_generic; a bare operator operand did not.)
            infer_generic(a, NULL);
            Type* arg_t = Type_Infer(a);
            if (arg_t) {
                unify_types(arg_t, node->call.sym->type->function.param_types[i],
                            decl->func_decl.type_params, inferred_args, param_count);
            }
        }

        // Top-down: from expected return type — fills any params still unbound
        // after typed args (e.g. `return identity(x)`, or `u32[3] r = make_arr(7)`
        // where the declared target should win over an untyped literal arg).
        // Runs AFTER typed args so real concrete types still take priority,
        // but BEFORE untyped literals so the target gets a shot at T first.
        if (target) {
            unify_types(target, node->call.sym->type->function.return_type,
                        decl->func_decl.type_params, inferred_args, param_count);
        }

        // Bottom-up pass 2 (previously promised by the pass-1 comment above but
        // never implemented): any type param STILL unbound after typed args and
        // the target both had their say gets pinned by an untyped literal arg,
        // using the literal's own default type (int -> i32, float -> f64) —
        // exactly the same default an untyped literal resolves to anywhere else
        // it lands without a more specific target (see coerce_literal_to_target).
        // This only fires for params nothing else could bind, so it can't
        // override a real concrete type or an explicit target; two disagreeing
        // untyped literals are still first-wins between each other here (KNOWN
        // BUG #6, unchanged, noted in the pass-1 comment above).
        for (size_t i = 0; i < node->call.arg_count && i < arg_count; i++) {
            ASTNode* a = node->call.args[i];
            bool is_int_lit = is_untyped_int_literal(a);
            bool is_float_lit = is_untyped_float_literal(a);
            if (!is_int_lit && !is_float_lit) continue;
            Type* param_t = node->call.sym->type->function.param_types[i];
            if (!param_t || param_t->cls != TYPE_PARAM) continue;
            Type* default_t = make_prim(is_float_lit ? PRIM_F64 : PRIM_I32);
            unify_types(default_t, param_t, decl->func_decl.type_params, inferred_args, param_count);
        }

        // Verification pass: re-unify every typed arg against the now-fully-inferred
        // type params to catch conflicts (e.g. same_type(u32_val, f32_val) where T
        // was bound to u32 by the first arg but f32 disagrees on the second).
        // Untyped literals are skipped here (they adapt; KNOWN BUG #6 covers them).
        for (size_t i = 0; i < node->call.arg_count && i < arg_count; i++) {
            if (is_untyped_int_literal(node->call.args[i]) ||
                is_untyped_float_literal(node->call.args[i]) ||
                is_null_literal(node->call.args[i])) continue;
            Type* arg_t = Type_Infer(node->call.args[i]);
            if (!arg_t || arg_t->cls == TYPE_PARAM) continue; // unresolved inner generic
            Type* param_t = node->call.sym->type->function.param_types[i];
            if (!param_t || param_t->cls != TYPE_PARAM) continue;
            // Only check params that are a direct type param (T), not compound (T*).
            // unify_types with a fully-inferred table returns false on mismatch.
            if (!unify_types(arg_t, param_t, decl->func_decl.type_params, inferred_args, param_count)) {
                // Find the conflicting type param name for a good error message.
                size_t idx = 0;
                for (; idx < param_count; idx++) {
                    if (strcmp(decl->func_decl.type_params[idx], param_t->param_name) == 0) break;
                }
                char got_buf[64], expected_buf[64];
                Type_ToString(arg_t, got_buf, sizeof(got_buf));
                Type_ToString(inferred_args[idx < param_count ? idx : 0], expected_buf, sizeof(expected_buf));
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf),
                        "type mismatch in argument %zu of call to '%s': "
                        "type parameter '%s' was inferred as '%s' but got '%s'",
                        i + 1, node->call.sym->name,
                        param_t->param_name, expected_buf, got_buf);
                Error_AtNode(node, errbuf, NULL);
            }
        }

        bool success = true;
        for (size_t i = 0; i < param_count; i++) {
            if (!inferred_args[i]) { success = false; break; }
        }

        if (success) {
            node->call.type_args = inferred_args;
            node->call.type_arg_count = param_count;
        } else {
            free(inferred_args);
        }
        return;
    }

    if (node->type == AST_IDENT) {
        if (node->ident.type_arg_count > 0) return; // already resolved
        if (!node->ident.sym || !node->ident.sym->generic_decl) return;
        if (!target || target->cls != TYPE_FUNCTION) return;

        ASTNode* decl = node->ident.sym->generic_decl;
        size_t param_count = decl->func_decl.type_param_count;
        if (param_count == 0) return;

        Type* generic_fn_type = node->ident.sym->type;
        if (!generic_fn_type || generic_fn_type->cls != TYPE_FUNCTION) return;

        Type** inferred = (Type**)calloc(param_count, sizeof(Type*));
        bool ok = unify_types(target, generic_fn_type,
                              decl->func_decl.type_params, inferred, param_count);
        if (!ok) { free(inferred); return; }

        for (size_t i = 0; i < param_count; i++) {
            if (!inferred[i]) { free(inferred); return; }
        }

        node->ident.type_args      = inferred;
        node->ident.type_arg_count = param_count;
        return;
    }
}

// Does this source node denote an UNTYPED literal constant (tier 2)? A bare integer
// or float literal has no committed type; it adapts to the destination if it fits.
// `300 + 1` or `(u8)300` are NOT this -- they have a computed/explicit type (tier 1).
// We require a *direct* literal so the rule is simple and predictable.
static bool is_untyped_int_literal(ASTNode* n) {
    return n && n->type == AST_INT_LITERAL && n->lit_kind == LIT_INT;
}
static bool is_untyped_float_literal(ASTNode* n) {
    return n && n->type == AST_INT_LITERAL && n->lit_kind == LIT_FLOAT;
}
static bool is_null_literal(ASTNode* n) {
    return n && n->type == AST_INT_LITERAL && n->lit_kind == LIT_NULL;
}
// Any of the three UNTYPED literal kinds -- takes its concrete type from
// context (an int/float literal with no fixed width yet, or `null` fitting
// any pointer) rather than carrying one of its own.
static bool is_untyped_literal(ASTNode* n) {
    return is_untyped_int_literal(n) || is_untyped_float_literal(n) || is_null_literal(n);
}

// Does the integer literal value `v` fit destination primitive `dst`? (unsigned dst:
// 0..2^w-1; signed dst: -2^(w-1)..2^(w-1)-1). Same rule as the const literal-fit.
static bool int_fits(int64_t v, const Type* dst) {
    int w = Type_Width(dst);
    if (w >= 8) return true;
    if (Type_IsSigned(dst)) {
        int64_t lo = -(1LL << (w * 8 - 1));
        int64_t hi =  (1LL << (w * 8 - 1)) - 1;
        return v >= lo && v <= hi;
    }
    if (v < 0) return false; // negative into unsigned needs a cast
    uint64_t hi = (1ULL << (w * 8)) - 1;
    return (uint64_t)v <= hi;
}

bool Type_IntLiteralFits(int64_t v, const Type* dst) { return int_fits(v, dst); }
Type* Type_MakePrim(int primitive_kind) { return make_prim((PrimitiveKind)primitive_kind); }

// Assignability check: is `src` allowed into a slot of type `dst`? Errors (exit) with
// a cast hint if not. Two tiers:
//   tier 1 -- a TYPED value (variable, expr, call): must be Type_Equals(dst, src) or
//             you need an explicit cast. No silent punning, no implicit widening.
//   tier 2 -- a DIRECT untyped literal: coerces into dst if the value fits. int lit ->
//             any int/float/pointer; float lit -> any float; null -> any pointer.
// `where` is a short label for the error ("declaration", "assignment", ...).
// Non-erroring core: same tiers/rules check_assignable has always had, but
// returns false on mismatch instead of calling Error_AtNode/exiting. Needed so
// a caller that ISN'T sure this is the final, real assignment (e.g. deciding
// whether `v = 5` means "call __assign" vs. plain assignment) can ask "would
// this fit?" without crashing the compiler on a "no" answer. check_assignable
// itself (below) is the thin, fatal wrapper every existing call site keeps
// using unchanged -- no dual logic, one real implementation.
static bool check_assignable_ne(Type* dst, ASTNode* src, const char* where) {
    // Type_Infer itself now hard-errors on AST_TYPE_EXPR (a bare type used in
    // value position -- see its case there for the full explanation), so this
    // is redundant as a SAFETY net; kept only because it fires first and gives
    // a message with `where` ("...in declaration of v") instead of the generic
    // one Type_Infer would produce a few lines down.
    if (src && src->type == AST_TYPE_EXPR) return false;

    Type* st = src ? Type_Infer(src) : NULL;
    if (!dst || !st) {
        // Same two-spellings-of-void normalization as Type_Equals' fn_ret_equal /
        // reflect_unify's Type_IsVoidLike check elsewhere: a bare `return` (src
        // NULL, so st NULL) against a function whose return type was written
        // EXPLICITLY as `void` (dst is a real PRIM_V Type*, not NULL) must be
        // treated as the SAME "nothing" on both sides, not a pointer-identity
        // mismatch between "no type" and "the void type." Without this, `return`
        // (no expression) only worked when the return type was OMITTED, and
        // hard-erred as "void vs value" the moment `void` was written explicitly
        // -- the one case an explicit `void` return type should behave IDENTICALLY
        // to an omitted one, since REFERENCE.md documents them as interchangeable.
        return Type_IsVoidLike(dst) && Type_IsVoidLike(st);
    }

    // Aggregates: brace literals resolve to dst before reaching here, so Type_Infer
    // returns the resolved type and Type_Equals succeeds. For other sources (variables,
    // calls, enum constructors) we fall through to the Type_Equals check below, which
    // catches mismatches like Option[u32] = Option[bool].

    // tier 2: direct untyped literals adapt to dst if they fit.
    if (is_null_literal(src)) return dst->cls == TYPE_POINTER;
    if (is_untyped_int_literal(src)) {
        if (coerce_literal_to_target(src, dst)) return true; // int literal -> float slot
        if (dst->cls == TYPE_PRIMITIVE) return int_fits((int64_t)src->int_value, dst);
    }
    if (is_untyped_float_literal(src)) return Type_IsFloat(dst);

    // tier 1: typed value -- must match exactly, else a cast is required.
    // For struct/array assignments: structural compatibility is verified during code gen.
    // Here we just ensure their names (or array shapes) match explicitly.
    //
    // A TYPE_FN_LITERAL source assigning into an ordinary `fn(...) T`-typed
    // destination is exactly the common case (`fn(i32,i32) bool cmp = asc`) --
    // the destination only cares about calling-convention shape, not which
    // specific function, so compare shapes there. A TYPE_FN_LITERAL destination
    // (a `Cmp`-typed slot after substitution) keeps the strict Type_Equals
    // nominal check below, since THAT position is exactly where identity is
    // supposed to matter.
    Type* cmp_st = (dst->cls != TYPE_FN_LITERAL) ? fn_lit_shape(st) : st;
    if (Type_Equals(dst, cmp_st)) return true;
    // User requested to relax the strict assignability check.
    // Allow implicit primitive coercions (like C).
    return dst->cls == TYPE_PRIMITIVE && cmp_st->cls == TYPE_PRIMITIVE;
}

// Thin fatal wrapper every existing call site keeps using unchanged: run the
// one real check (check_assignable_ne), and if it says no, error out exactly
// the way this function always has. `dst`/`src` are re-inspected here ONLY to
// pick which of the existing message templates to print -- no assignability
// RULE is duplicated (check_assignable_ne already decided pass/fail; this is
// pure message selection on a known-failing case).
static void check_assignable(Type* dst, ASTNode* src, const char* where) {
    if (check_assignable_ne(dst, src, where)) return;

    if (src && src->type == AST_TYPE_EXPR) {
        char msg[160];
        snprintf(msg, sizeof(msg), "a type cannot be used as a value in %s", where);
        Error_AtNode(src, msg, NULL);
    }
    Type* st = src ? Type_Infer(src) : NULL;
    if (!dst || !st) {
        char msg[256];
        snprintf(msg, sizeof(msg), "type mismatch in %s: void vs value", where);
        Error_AtNode(src, msg, NULL);
    }
    if (is_null_literal(src)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "null in %s requires a pointer type", where);
        Error_AtNode(src, msg, NULL);
    }
    // Same guard check_assignable_ne uses: this message only applies when dst
    // is a primitive slot the literal could have fit (and didn't) -- a struct
    // dst (`P p = 5`) falls through to the generic "cannot assign" message
    // below instead, exactly like the original tier-2 code did.
    if (is_untyped_int_literal(src) && dst->cls == TYPE_PRIMITIVE &&
        !coerce_literal_to_target(src, dst)) {
        char msg[192];
        snprintf(msg, sizeof(msg),
                 "integer literal in %s does not fit the destination type (use a cast to truncate)",
                 where);
        Error_AtNode(src, msg, NULL);
    }
    if (is_untyped_float_literal(src)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "float literal in %s requires a float destination (use a cast)", where);
        Error_AtNode(src, msg, NULL);
    }
    Type* cmp_st = (dst->cls != TYPE_FN_LITERAL) ? fn_lit_shape(st) : st;
    char vt_buf[128], it_buf[128], msg[320];
    Type_ToString(dst, vt_buf, sizeof(vt_buf));
    Type_ToString(cmp_st, it_buf, sizeof(it_buf));
    snprintf(msg, sizeof(msg),
             "type mismatch in %s: cannot assign %s to %s without an explicit cast",
             where, it_buf, vt_buf);
    Error_AtNode(src, msg, NULL);
}

// Eager typecheck/annotation pass. Walks statements and sub-expressions so every
// node carries its result_type before codegen runs. This is the single home for
// the v1(b) conversion rules (narrowing-needs-cast, literal-fit) when they land:
// add the checks here, where the full tree and declared types are in view.
// Does every path through this statement end in a `return`? Used only to reject a
// non-void function that can fall off its own end (see AST_FUNC_DECL below).
//
// Deliberately CONSERVATIVE: it answers "provably yes" or "not provably yes", never
// "provably no". A false negative costs the author one unreachable `return`; a false
// positive would let junk through, which is the thing being fixed. So:
//
//   - a loop is NOT counted, even `while true { return 1 }`, because proving a loop
//     always runs its body (and never `break`s) is a dataflow analysis, and this is a
//     syntactic check. Write the trailing return.
//   - a `match` needs no case of its own: it lowers to a chain of AST_IF (§17.4), so
//     the if/else rule below decides it. An `else` arm is what supplies the final
//     false_block, which is why a `match` without one is (correctly) rejected.
static bool stmt_always_returns(ASTNode* n) {
    if (!n) return false;
    switch (n->type) {
        case AST_RETURN:
            return true;
        case AST_BLOCK:
            // Any statement that always returns makes the rest of the block dead, so
            // the block as a whole always returns.
            for (size_t i = 0; i < n->block.count; i++)
                if (stmt_always_returns(n->block.statements[i])) return true;
            return false;
        case AST_IF:
            // A bare `if` with no `else` always has a path that skips it -- EXCEPT when
            // it is the final arm of an exhaustive `match`. That arm keeps its condition
            // in the lowering (`else if (tag == 2) { C }`) purely as an artifact, so it
            // has no false_block while still being reached by every value. Without this
            // case, `match e { .A{x} { return x }  .None { return 0 } }` -- exhaustive,
            // every arm returns -- would be reported as falling off the end.
            if (!n->if_stmt.false_block)
                return n->if_stmt.exhaustive_tail &&
                       stmt_always_returns(n->if_stmt.true_block);
            return stmt_always_returns(n->if_stmt.true_block) &&
                   stmt_always_returns(n->if_stmt.false_block);
        default:
            return false;
    }
}

// Forward decl: defined below, but Typecheck_Tree's AST_IF case (a non-generic
// `match T`'s resolution path) needs to call it -- see that case for why.
static void Resolve_Reflect_Matches(ASTNode* n);

// Core of a `match T` AST_IF's resolution, shared by Typecheck_Tree (the
// non-generic path) and Resolve_Reflect_Matches (the post-clone_ast path):
// unify the scrutinee against the pattern, splice `node` in place into
// either the taken arm (cloning it with any reflect bindings) or the
// else/empty fallback. Returns false (no-op) when the scrutinee is still an
// unresolved TYPE_PARAM -- not yet substituted, defer to a later pass -- or
// true once `node` has become the selected block, ready for the caller's own
// follow-up. A NULL scrutinee is NOT "still generic": it's also the genuine
// resolved value of an omitted type (a no-payload enum variant's H) -- only
// bail on an ACTUAL TYPE_PARAM node.
static bool reflect_match_select(ASTNode* node) {
    Type* scrut = node->if_stmt.reflect_scrutinee;
    if (scrut && scrut->cls == TYPE_PARAM) return false;
    ReflectBindings binds = {0};
    if (reflect_unify(scrut, node->if_stmt.reflect_pattern, &binds)) {
        ASTNode* body = node->if_stmt.true_block;
        if (binds.count > 0) body = clone_ast(body, binds.names, binds.args, binds.count, false);
        reflect_bindings_free(&binds);
        *node = *body;
    } else {
        reflect_bindings_free(&binds);
        ASTNode* rest = node->if_stmt.false_block;
        if (rest) {
            *node = *rest;
        } else {
            node->type = AST_BLOCK;
            node->block.statements = NULL;
            node->block.count = 0;
            node->block.capacity = 0;
        }
    }
    return true;
}

void Typecheck_Tree(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->block.count; i++)
                Typecheck_Tree(node->block.statements[i]);
            return;

        case AST_FUNC_DECL: {
            if (node->func_decl.type_param_count > 0) return;
            Type* prev = s_current_fn_return;
            s_current_fn_return = node->func_decl.return_type;
            Typecheck_Tree(node->func_decl.body);
            s_current_fn_return = prev;

            // A non-void function whose control flow can reach the closing brace
            // returns whatever happened to be in the return register -- `fn f() u32 {}`
            // silently produced junk, as did `if c { return 1 }` with no else, and a
            // `match` with no arm taken and no `else`. That is silent wrongness (§1),
            // and it is the one failure mode this language exists to not have.
            //
            // `match` needs no special case here: it lowers to a chain of AST_IF (§17.4),
            // so "every path returns" over if/else already answers it. A `match` with an
            // `else` whose arms all return has a false_block on the last link and passes;
            // one without an `else` does not, and is rejected -- which is exactly right,
            // since a type-match cannot be exhaustive over the infinite set of types and
            // `else` is how the language already spells the catch-all (§23).
            // (`void` parses to PRIM_V, not PRIM_VOID -- both live in the enum, and
            // parser.c:514 produces PRIM_V. Testing only PRIM_VOID silently demanded a
            // return from every void function, which is how this first came out wrong.)
            Type* rt = node->func_decl.return_type;
            bool returns_void = rt && rt->cls == TYPE_PRIMITIVE &&
                                (rt->primitive == PRIM_V || rt->primitive == PRIM_VOID);
            if (node->func_decl.body && rt && !returns_void &&
                !stmt_always_returns(node->func_decl.body)) {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf),
                        "control reaches the end of non-void function '%.*s' "
                        "without a return",
                        (int)node->func_decl.name_len, node->func_decl.name);
                Error_AtNode(node, errbuf, NULL);
            }
            return;
        }

        case AST_IF:
            // `match T` resolution: shared with Resolve_Reflect_Matches (that
            // copy runs eagerly right after a generic clone; this one is the
            // ONLY place ordinary non-generic code's `match` ever resolves,
            // since Resolve_Reflect_Matches only runs on generic bodies).
            // reflect_match_select splices node into its taken arm (or the
            // empty/else fallback) in place; the taken branch's OWN nested
            // matches still need resolving (Resolve_Reflect_Matches, since
            // clone_ast doesn't recurse into them), only then Typecheck_Tree.
            if (node->if_stmt.reflect_pattern) {
                if (reflect_match_select(node)) {
                    Resolve_Reflect_Matches(node);
                    Typecheck_Tree(node);
                }
                return;
            }
            Typecheck_Tree(node->if_stmt.condition);
            // Real if-constexpr: if the condition (now typechecked, and for
            // a type-vs-type comparison like `T == i32`, already folded to a
            // literal bool by the AST_EQ/AST_NEQ fold above) is a compile-
            // time-constant bool, the untaken branch is never typechecked at
            // all — not just never compiled. This is what makes `if T ==
            // i32 { ... } else { ... }` genuinely useful for generic
            // specialization: without it, the untaken branch referencing
            // something only valid for the OTHER instantiation is still a
            // hard compile error (confirmed by testing before adding this),
            // which defeats the entire point of branching on a per-
            // instantiation type fact. This never INVENTS foldability —
            // it only recognizes a condition that's already a plain
            // AST_INT_LITERAL/LIT_BOOL after Typecheck_Tree ran on it once,
            // the same "already folded, so skip the rest" pattern every
            // other constant-fold in this file already follows.
            if (node->if_stmt.condition &&
                node->if_stmt.condition->type == AST_INT_LITERAL &&
                node->if_stmt.condition->lit_kind == LIT_BOOL) {
                if (node->if_stmt.condition->int_value != 0) {
                    Typecheck_Tree(node->if_stmt.true_block);
                } else if (node->if_stmt.false_block) {
                    Typecheck_Tree(node->if_stmt.false_block);
                }
                return;
            }
            Typecheck_Tree(node->if_stmt.true_block);
            Typecheck_Tree(node->if_stmt.false_block);
            return;

        case AST_WHILE:
            Typecheck_Tree(node->while_stmt.condition);
            Typecheck_Tree(node->while_stmt.body);
            return;

        case AST_FOR:
            Typecheck_Tree(node->for_stmt.init);
            Typecheck_Tree(node->for_stmt.cond);
            Typecheck_Tree(node->for_stmt.incr);
            Typecheck_Tree(node->for_stmt.body);
            return;

        case AST_DEFER:
            Typecheck_Tree(node->unary);
            return;

        case AST_RETURN:
            // Generic call or bare generic function name, resolved against the
            // function's own declared return type.
            if (node->unary) infer_generic(node->unary, s_current_fn_return);
            // A `return {...}` bare literal resolves against the function's return type.
            if (node->unary) resolve_brace_literal(node->unary, s_current_fn_return);
            Typecheck_Tree(node->unary);
            // Assignability: the returned value must be allowed into the return type.
            check_assignable(s_current_fn_return, node->unary, "return");
            return;

        case AST_NEW:
            if (node->new_expr.init)  Typecheck_Tree(node->new_expr.init);
            if (node->new_expr.count) Typecheck_Tree(node->new_expr.count);
            Type_Infer(node);
            return;

        case AST_DELETE: {
            // Destructor hook: `delete p` calls p's __delete() (if its pointee
            // type defines one) before the actual free -- the language's real
            // free-exit-point (see std/vector.t's Vector[T].__delete for the
            // reference convention: recurse into owned elements/buffers, THEN
            // this node's own unconditional free(self.data) releases the raw
            // storage). Reuses ordinary method-call dispatch (try_rewrite_
            // method_call, ordinary AST_CALL codegen) for the __delete() call
            // itself; the actual pointer release stays exactly the existing,
            // unmodified AST_DELETE codegen (a plain free(), always -- __delete
            // is cleanup logic, never a replacement for releasing the memory).
            Typecheck_Tree(node->delete_expr.ptr);
            Type* pt = Type_Infer(node->delete_expr.ptr);
            Type* pointee = (pt && pt->cls == TYPE_POINTER) ? pt->pointer_base : NULL;
            if (pointee && pointee->cls == TYPE_STRUCT &&
                Method_Resolve(pointee, "__delete", 8)) {
                ASTNode* target = (ASTNode*)calloc(1, sizeof(ASTNode));
                target->type = AST_FIELD;
                target->field.base = node->delete_expr.ptr;
                target->field.field_name = "__delete";
                target->field.field_name_len = 8;
                target->line = node->line;
                target->column = node->column;
                ASTNode* call = (ASTNode*)calloc(1, sizeof(ASTNode));
                call->type = AST_CALL;
                call->call.target_expr = target;
                call->line = node->line;
                call->column = node->column;
                // orig_delete is a COPY of this node, still AST_DELETE,
                // reusing its exact existing (unmodified) codegen -- it must
                // NOT be re-typechecked through this same AST_DELETE case
                // (Typecheck_Tree(node) recursing into it would just find
                // __delete again and rewrite it again, forever). Its pointer
                // expression was already typechecked above (same expr,
                // shared by both this node and the copy); nothing else in
                // AST_DELETE's codegen needs anything from typecheck.
                ASTNode* orig_delete = (ASTNode*)calloc(1, sizeof(ASTNode));
                *orig_delete = *node;

                Typecheck_Tree(call);

                node->type = AST_BLOCK;
                node->block.capacity = 2;
                node->block.count = 2;
                node->block.statements = (ASTNode**)malloc(2 * sizeof(ASTNode*));
                node->block.transparent = true;
                node->block.statements[0] = call;
                node->block.statements[1] = orig_delete;
            }
            return;
        }

        case AST_DECLARATION:
            // Generic call or bare generic function name, resolved against the
            // declared variable's type, e.g. `fn(u32) u32 f = identity`.
            if (node->decl.init_expr) infer_generic(node->decl.init_expr, node->decl.var_type);
            // A bare `{...}` initializer resolves against the declared variable type.
            if (node->decl.init_expr) resolve_brace_literal(node->decl.init_expr, node->decl.var_type);
            Typecheck_Tree(node->decl.init_expr);
            // Assignability: the initializer must be allowed into the declared type.
            if (node->decl.init_expr) {
                char buf[256];
                snprintf(buf, sizeof(buf), "declaration of %.*s",
                     node->decl.name ? (int)node->decl.name_len : 7,
                     node->decl.name ? node->decl.name : "unknown");
                check_assignable(node->decl.var_type, node->decl.init_expr, buf);
            }
            return;

        case AST_CALL:
            // Deferred resolution: callee may be defined later in the file.
            if (node->call.target_expr) {
                // impl method call: `expr.method(args)` parses as target_expr=AST_FIELD.
                // If the field doesn't exist in the struct, try looking up Foo_method.
                try_rewrite_method_call(node);
                if (node->call.target_expr) {
                    Typecheck_Tree(node->call.target_expr);
                    Type* call_tgt = Type_FnLitShape(Type_Infer(node->call.target_expr));
                    if (call_tgt && call_tgt->cls == TYPE_POINTER && call_tgt->pointer_base && call_tgt->pointer_base->cls == TYPE_FUNCTION) {
                        call_tgt = call_tgt->pointer_base;
                    }
                    if (!call_tgt || call_tgt->cls != TYPE_FUNCTION) {
                        // Call-operator overload: same rewrite as Type_Infer's
                        // AST_CALL case (see try_rewrite_call_operator) -- this is
                        // a SEPARATE typecheck pass over the same AST_CALL shape,
                        // so it needs the identical fallback before erroring.
                        if (try_rewrite_call_operator(node, call_tgt)) {
                            try_rewrite_method_call(node);
                            Typecheck_Tree(node);
                            return;
                        }
                        Error_AtNode(node, "calling non-function", NULL);
                    }
                }
                if (!node->call.target_expr && node->call.sym) goto call_sym_resolved;
            } else if (!node->call.sym) {
                node->call.sym = SymTable_Find(Get_SymTable(),
                                               node->call.target_name,
                                               node->call.target_name_len);
                if (!node->call.sym || node->call.sym->kind != SYM_FUNCTION) {
                    char msg[160];
                    snprintf(msg, sizeof(msg), "undeclared function %.*s",
                             (int)node->call.target_name_len, node->call.target_name);
                    Error_AtNode(node, msg, NULL);
                }
            }
            call_sym_resolved:;
            // If the call hasn't been inferred top-down yet, attempt bottom-up inference now
            infer_generic(node, NULL);
            // Resolve a bare `{...}` argument against the matching parameter type.
            // Available for an ordinary (non-target-expr) function whose symbol carries
            // a TYPE_FUNCTION with param_types. Generic params (TYPE_PARAM) are skipped
            // here -- a literal arg to a generic param isn't supported in v1.
            {
                bool has_sym =
                    node->call.sym && node->call.sym->type &&
                    node->call.sym->type->cls == TYPE_FUNCTION;
                bool is_generic = has_sym && node->call.sym->generic_decl;
                bool generic_resolved = is_generic && node->call.type_arg_count > 0;

                if (is_generic && !generic_resolved) {
                    char msg[192];
                    snprintf(msg, sizeof(msg),
                             "cannot infer generic type arguments for call to '%.*s'",
                             (int)node->call.sym->name_len, node->call.sym->name);
                    Error_AtNode(node, msg, NULL);
                }

                // Build the concrete parameter type list.
                // For generic calls with resolved type args, substitute TYPE_PARAM → concrete.
                Type** ptypes = NULL;
                size_t pcount = 0;
                bool is_vararg = false;
                bool can_check_params = false;

                if (has_sym && (!is_generic || generic_resolved)) {
                    can_check_params = true;
                    pcount = node->call.sym->type->function.param_count;
                    is_vararg = node->call.sym->type->function.is_vararg;

                    if (generic_resolved) {
                        Generic_Instantiate(node->call.sym, node->call.type_args, node->call.type_arg_count);
                        ASTNode* gdecl = node->call.sym->generic_decl;
                        ptypes = (Type**)malloc(pcount * sizeof(Type*));
                        for (size_t i = 0; i < pcount; i++) {
                            ptypes[i] = Type_Substitute(
                                node->call.sym->type->function.param_types[i],
                                gdecl->func_decl.type_params,
                                node->call.type_args,
                                node->call.type_arg_count);
                        }
                    } else {
                        ptypes = node->call.sym->type->function.param_types;
                    }
                } else if (node->call.target_expr) {
                    // INDIRECT call through a function pointer: get the param types
                    // from the fnptr's type so args get coerced too (e.g. an int
                    // literal -> f32 param). Without this, indirect-call args skipped
                    // coercion and an int lit stayed integer bits in a float param.
                    // fn_lit_shape: the target may be a TYPE_FN_LITERAL (a generic
                    // param bound to a specific function, e.g. `cmp` after `Cmp`
                    // substitution) -- unwrap to its shape for this shape-only check.
                    Type* ft = fn_lit_shape(Type_Infer(node->call.target_expr));
                    if (ft && ft->cls == TYPE_POINTER && ft->pointer_base &&
                        ft->pointer_base->cls == TYPE_FUNCTION) ft = ft->pointer_base;
                    if (ft && ft->cls == TYPE_FUNCTION) {
                        can_check_params = true;
                        pcount = ft->function.param_count;
                        is_vararg = ft->function.is_vararg;
                        ptypes = ft->function.param_types;
                    }
                }

                if (can_check_params) {
                    if (is_vararg) {
                        if (node->call.arg_count < pcount) {
                            Error_AtNode(node, "not enough arguments to vararg function", NULL);
                        }
                    } else {
                        if (node->call.arg_count != pcount) {
                            char msg[192];
                            snprintf(msg, sizeof(msg), "argument count mismatch in call to %.*s",
                                     (int)node->call.sym->name_len, node->call.sym->name);
                            Error_AtNode(node, msg, NULL);
                        }
                    }
                }

                // Resolve bare `{...}` args against their param types.
                // Also resolve generic function idents in value position (e.g. apply(identity, 42)).
                // Also: if an argument is itself a call to an unresolved generic function
                // (e.g. b(a(20)) where a's T can't be inferred from a's own args), pin its
                // type params top-down from the expected parameter type here -- this is the
                // same top-down inference already done for return/decl/assign targets, just
                // applied to call-argument position too.
                for (size_t i = 0; ptypes && i < node->call.arg_count && i < pcount; i++) {
                    if (ptypes[i] && ptypes[i]->cls != TYPE_PARAM) {
                        resolve_brace_literal(node->call.args[i], ptypes[i]);
                        infer_generic(node->call.args[i], ptypes[i]);
                    }
                }
                for (size_t i = 0; i < node->call.arg_count; i++)
                    Typecheck_Tree(node->call.args[i]);
                // Assignability: each arg must be allowed into its parameter type.
                for (size_t i = 0; ptypes && i < node->call.arg_count && i < pcount; i++) {
                    if (ptypes[i] && ptypes[i]->cls != TYPE_PARAM)
                        check_assignable(ptypes[i], node->call.args[i], "function argument");
                }
            }
            Type_Infer(node);
            return;

        case AST_ASSIGN:
            // Assignment-operator overload (v = 5 where Vector defines
            // __assign(i32)) MUST be tried before check_assignable below --
            // unlike the other operators, the entire point of __assign is to
            // accept an RHS that would NOT otherwise fit the lvalue's type
            // (that's what "v = 5" on a struct means), so if check_assignable
            // ran first it would reject the RHS as a mismatch before __assign
            // ever got a chance to run. try_rewrite_operator_method (the SAME
            // shared core Type_Infer/infer_generic also call -- see its own
            // AST_ASSIGN branch) already decides whether __assign genuinely
            // applies here (vs. falling back to plain same-type assignment,
            // like `b = a`), so this is just the ordering fix, no separate gate.
            if (try_rewrite_operator_method(node)) {
                Typecheck_Tree(node);
                return;
            }
            Typecheck_Tree(node->binary.left);
            // Generic call or bare generic function name on the RHS, resolved
            // against the lvalue's type, e.g. `f = identity` where f is fn(u32) u32.
            infer_generic(node->binary.right, Type_Infer(node->binary.left));
            // A bare `{...}` RHS resolves against the lvalue's type.
            resolve_brace_literal(node->binary.right, Type_Infer(node->binary.left));
            Typecheck_Tree(node->binary.right);
            // Assignability: the RHS must be allowed into the lvalue's type.
            check_assignable(Type_Infer(node->binary.left), node->binary.right, "assignment");
            Type_Infer(node);
            return;

        case AST_DEREF: {
            Typecheck_Tree(node->unary);
            Type* base = Type_Infer(node->unary);
            // *expr requires expr to be a pointer. Type_Infer's own AST_DEREF case
            // already detects this (returns NULL when base isn't TYPE_POINTER) but
            // treats it as "type unknown" rather than an error — every downstream
            // consumer (codegen, constexpr, the LLVM backend) then silently treated
            // that NULL as "proceed anyway," so *7 compiled, then dereferenced the
            // literal integer 7 as if it were a real address and crashed at runtime.
            // The language already has an explicit, sanctioned way to treat an
            // integer as an address (cast to a pointer type first, e.g. *(i32*)x) —
            // this is the one place a reinterpretation was happening silently,
            // without a cast, unlike everywhere else in the type system.
            if (!base || base->cls != TYPE_POINTER) {
                char tbuf[256], msg[320];
                Type_ToString(base, tbuf, sizeof(tbuf));
                snprintf(msg, sizeof(msg),
                         "cannot dereference non-pointer type '%s' "
                         "(cast to a pointer type first, e.g. *(T*) expr)", tbuf);
                Error_AtNode(node, msg, NULL);
            }
            Type_Infer(node);
            return;
        }

        case AST_ADDR: case AST_LOGICAL_NOT: case AST_BIT_NOT:
            Typecheck_Tree(node->unary);
            Type_Infer(node);
            return;

        case AST_CAST:
            // A cast supplies a target type, so a bare `{...}` / `.Variant` operand
            // resolves against it — `(P){.x=1}` / `(u32[3]){1,2,3}`. This is the one
            // construct that gives a literal a type where the surrounding context has
            // none (no decl/param/return target), e.g. a variadic call argument.
            // Same resolver every other target site uses; the cast just forgot it.
            resolve_brace_literal(node->cast.expr, node->cast.target_type);
            infer_generic(node->cast.expr, node->cast.target_type);
            Typecheck_Tree(node->cast.expr);
            // Cast-operator overload: `(T)x` where x's type defines a generic
            // __cast[T]() method. Tried AFTER the operand is fully resolved
            // above (a literal operand needs a real type before Method_Resolve
            // can look anything up on it) but BEFORE the built-in struct-
            // downcast logic below, same "overload takes priority over
            // built-in aggregate behavior" precedent __add/__eq/etc. already
            // set against lanewise arithmetic/comparison.
            if (try_rewrite_cast_operator(node)) {
                try_rewrite_method_call(node);
                infer_generic(node, NULL);
                Typecheck_Tree(node);
                return;
            }
            // Casting a bare TYPE used in value position (`(i32)u64`, or `(i32)P`
            // where P is a match-pattern type wildcard) is nonsensical: a type has
            // no value to cast. Without this guard the AST_TYPE_EXPR operand reaches
            // codegen, which has no case for it, and crashes (pre-existing — see
            // docs/KNOWN_BUGS.md). Catch it here with a clear message. A value-param
            // wildcard (`N`) is fine — it's already lowered to a literal, not an
            // AST_TYPE_EXPR — so only a genuine type-in-value-position trips this.
            if (node->cast.expr && node->cast.expr->type == AST_TYPE_EXPR) {
                Error_AtNode(node, "a type cannot be used as a value here (cannot cast a bare type)", NULL);
            }
            // (Comment deleted, casting to function types now allowed)
            
            Type* src = Type_Infer(node->cast.expr);
            Type* tgt = node->cast.target_type;
            if (tgt && tgt->cls == TYPE_STRUCT) {
                if (!src || src->cls != TYPE_STRUCT) {
                    Error_AtNode(node, "cannot cast a non-struct to a struct", NULL);
                }
                StructDef* s_tgt = Struct_Find(tgt->struct_name);
                StructDef* s_src = Struct_Find(src->struct_name);
                if (s_tgt && s_src) {
                    Struct_Layout(s_src);
                    Struct_Layout(s_tgt);
                    if (s_tgt->field_count > s_src->field_count) {
                        Error_AtNode(node, "cannot cast to a struct with more fields than the source", NULL);
                    }
                    size_t off = s_src->field_count - s_tgt->field_count;
                    for (size_t i = 0; i < s_tgt->field_count; i++) {
                        if (!Type_Equals(s_tgt->fields[i].type, s_src->fields[off + i].type)) {
                            Error_AtNode(node, "struct cast target must perfectly match a suffix of the source fields", NULL);
                        }
                    }
                    if (s_tgt->field_count > 0) {
                        node->cast.struct_downcast_offset = s_src->fields[off].offset;
                    } else {
                        node->cast.struct_downcast_offset = 0;
                    }
                }
            }

            Type_Infer(node);
            return;

        case AST_FIELD:
            Typecheck_Tree(node->field.base);
            Type_Infer(node);
            return;

        case AST_STRUCT_LITERAL:
            // Enum variant construction: its payload (if any) is a value for the variant
            // field, so resolve a nested bare `{...}` payload against the variant's type
            // (the variant is fields[idx]; its `type` is the payload type). Reuses the
            // same resolver as struct fields -- the only enum-specific typecheck line.
            if (node->struct_lit.sdef && node->struct_lit.sdef->is_enum && node->struct_lit.count == 1) {
                StructField* v = Struct_FindField(node->struct_lit.sdef,
                                                  node->struct_lit.field_names[0],
                                                  node->struct_lit.field_name_lens[0]);
                if (v && v->type) resolve_brace_literal(node->struct_lit.values[0], v->type);
            }
            // General (non-enum) case: this struct literal may already have sdef set
            // (e.g. `new Point{.x=.., .y=..}` -- alloc_type supplies it directly, never
            // going through resolve_brace_literal's sdef==NULL path), so look up each
            // field's own type here too -- otherwise a field value that's itself an
            // unresolved generic call never gets a target to pin against.
            if (node->struct_lit.sdef && !node->struct_lit.sdef->is_enum) {
                for (size_t i = 0; i < node->struct_lit.count; i++) {
                    StructField* f = Struct_FindField(node->struct_lit.sdef,
                                                      node->struct_lit.field_names[i],
                                                      node->struct_lit.field_name_lens[i]);
                    if (f && f->type) {
                        resolve_brace_literal(node->struct_lit.values[i], f->type);
                        infer_generic(node->struct_lit.values[i], f->type);
                    }
                }
            }
            for (size_t i = 0; i < node->struct_lit.count; i++)
                Typecheck_Tree(node->struct_lit.values[i]);
            if (!Type_Infer(node)) {
                char msg[320];
                if (node->struct_lit.is_enum_variant)
                    snprintf(msg, sizeof(msg),
                             "cannot infer the enum type of `.%.*s`; the target type "
                             "is unknown (e.g. an un-inferred generic parameter). Bind it to a "
                             "typed variable or pass an explicit type argument.",
                             (int)node->struct_lit.field_name_lens[0], node->struct_lit.field_names[0]);
                else
                    snprintf(msg, sizeof(msg),
                             "cannot infer the type of this literal; the target type "
                             "is unknown (e.g. an un-inferred generic parameter). Bind it to a typed "
                             "variable or pass an explicit type argument.");
                Error_AtNode(node, msg, NULL);
            }
            return;

        case AST_INDEX:
            Typecheck_Tree(node->index.base);
            Typecheck_Tree(node->index.index);
            Type_Infer(node);
            return;

        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->array_lit.count; i++)
                Typecheck_Tree(node->array_lit.values[i]);
            if (!Type_Infer(node)) {
                Error_AtNode(node,
                    "cannot infer the type of this literal; the target type "
                    "is unknown (e.g. an un-inferred generic parameter). Bind it to a typed "
                    "variable or pass an explicit type argument.",
                    NULL);
            }
            return;

        case AST_STRUCT_DECL:
            return; // no sub-expressions; layout already done at parse time

        case AST_SIZEOF:
        case AST_ALIGNOF:
        case AST_OFFSETOF:
        case AST_NAMEOF:
            Type_Infer(node); // value already resolved at parse time
            return;

        case AST_IDENT:
            // Deferred resolution: identifier may refer to a function defined later in the file.
            if (!node->ident.sym) {
                node->ident.sym = SymTable_Find(Get_SymTable(),
                                                node->ident.name,
                                                node->ident.name_len);
                if (!node->ident.sym) {
                    char msg[160];
                    snprintf(msg, sizeof(msg), "undeclared identifier %.*s",
                             (int)node->ident.name_len, node->ident.name);
                    Error_AtNode(node, msg, NULL);
                }
            }
            if (node->ident.sym && node->ident.sym->generic_decl && node->ident.type_arg_count > 0) {
                Generic_Instantiate(node->ident.sym, node->ident.type_args, node->ident.type_arg_count);
            }
            Type_Infer(node);
            return;

        case AST_BREAK: case AST_CONTINUE:
            return;

        default:
            // A bare type used as an operand of ANY binary operator (`T == i32`,
            // `T * 2`, `T < 5`, `T && x`, ...) is REMOVED, not just unimplemented
            // for ==/!=. AST_TYPE_EXPR has no codegen case at all -- it isn't a
            // value, it can't be loaded into a register, added, compared, or
            // anything else. This used to be checked for == and != only (a fold
            // path existed for "both sides are types", so only a MISMATCHED
            // comparison was rejected) -- but every other operator (*, +, <, &&,
            // ...) fell through this same default case with no AST_TYPE_EXPR
            // awareness at all, reaching an unrelated, confusing failure further
            // down (e.g. a `return T * 2` surfacing as "void vs value" from the
            // return's own assignability check, nothing to do with the real
            // problem). One check for every binary op, not one per operator: if
            // EITHER operand is a bare type, this operator has nothing to work
            // on, full stop. The sanctioned way to branch on a type is `match T
            // { u32 {..} else {..} }` (§17.4), which never reaches this code at
            // all -- it lowers straight to reflect_pattern/reflect_scrutinee on
            // an AST_IF (see parser.c parse_match_type), not to any binary op.
            if (node->type >= AST_ADD && node->type <= AST_LOGICAL_OR &&
                node->binary.left && node->binary.right &&
                (node->binary.left->type == AST_TYPE_EXPR ||
                 node->binary.right->type == AST_TYPE_EXPR)) {
                Error_AtNode(node, "cannot use a bare type as an operand of an operator "
                                   "(use `match T { ... }` to branch on a type instead)", NULL);
            }
            // Binary operators and leaves: recurse into operands if present, then infer.
            if (node->type >= AST_ADD && node->type <= AST_LOGICAL_OR) {
                // Bottom-up generic inference (from args) doesn't need a target type,
                // so run it early on either side if it's an unresolved generic call --
                // this lets Type_Infer below see the real (substituted) type instead
                // of a bare TYPE_PARAM.
                if (node->binary.left && node->binary.left->type == AST_CALL)
                    Typecheck_Tree(node->binary.left);
                if (node->binary.right && node->binary.right->type == AST_CALL)
                    Typecheck_Tree(node->binary.right);

                // A bare `{...}` on either side resolves against the other side's
                // (already-known) type, same as AST_ASSIGN does for its RHS.
                // Guard: TYPE_PARAM means the other side is still unresolved
                // (e.g. target-type-dependent generic) -- skip rather than
                // resolving the literal to garbage.
                if (node->binary.left && node->binary.right) {
                    Type* rt = Type_Infer(node->binary.right);
                    if (rt && rt->cls != TYPE_PARAM) resolve_brace_literal(node->binary.left, rt);
                    Type* lt = Type_Infer(node->binary.left);
                    if (lt && lt->cls != TYPE_PARAM) resolve_brace_literal(node->binary.right, lt);
                }
                Typecheck_Tree(node->binary.left);
                Typecheck_Tree(node->binary.right);

                Type* lt = Type_Infer(node->binary.left);
                Type* rt = Type_Infer(node->binary.right);

                // A user-defined operator method (__add, __eq, ...) on the LEFT
                // operand's struct type takes priority over built-in lanewise
                // aggregate arithmetic below -- defer entirely to Type_Infer's
                // own s_op_methods rewrite (a+b -> a.__add(b)) at the bottom of
                // this function instead of treating the struct as a lanewise
                // operand here. Without this, a struct with __add still got
                // silently lanewise-added: this check runs BEFORE Type_Infer(node)
                // is ever called on the AST_ADD node itself (only on its operands
                // above), so the rewrite never had a chance to fire first.
                bool has_op_method = node->type < (ASTNodeType)(sizeof(s_op_methods)/sizeof(s_op_methods[0]))
                    && s_op_methods[node->type] && lt
                    && Method_Resolve(lt, s_op_methods[node->type], strlen(s_op_methods[node->type]));

                if (!has_op_method && (Type_IsAggregate(lt) || Type_IsAggregate(rt))) {
                    if (!lt || !rt || !Type_Equals(lt, rt)) {
                        Error_AtNode(node,
                            "binary operator on mismatched operand types "
                            "(aggregate operators require both sides to be the same "
                            "struct or array type)",
                            NULL);
                    }
                    bool is_eq = (node->type == AST_EQ || node->type == AST_NEQ);
                    if (!is_eq) {
                        const char* overload_name = "an overload method";
                        if (node->type < (ASTNodeType)(sizeof(s_op_methods)/sizeof(s_op_methods[0])) && s_op_methods[node->type]) {
                            overload_name = s_op_methods[node->type];
                        }
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "operator not defined on aggregate operands "
                                 "(implicit lanewise arithmetic has been removed; implement %s instead)",
                                 overload_name);
                        Error_AtNode(node, msg, NULL);
                    }
                    uint64_t dummy_offs[256], dummy_ws[256];
                    int nl = Agg_Lanes(lt, 0, dummy_offs, dummy_ws, 0, 256);
                    if (nl < 0) {
                        char msg[192];
                        snprintf(msg, sizeof(msg),
                                 "operator not defined on %s operands "
                                 "(lanewise comparison failed: aggregate contains unsupported types like pointers)",
                                 lt->cls==TYPE_STRUCT?"struct":"array");
                        Error_AtNode(node, msg, NULL);
                    }
                }

                if (Type_IsFloat(lt) || Type_IsFloat(rt)) {
                    bool valid = false;
                    switch (node->type) {
                        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV:
                        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
                            valid = true; break;
                        default: break;
                    }
                    if (!valid) {
                        Error_AtNode(node, "operator not defined for floating-point operands", NULL);
                    }
                }
            }
            Type_Infer(node);
            return;
    }
}

// --- type-arg key, for caching by (generic symbol, concrete type tuple) ---
static void type_keystr(Type* t, char* out, size_t cap);

typedef struct {
    Symbol* generic;     // the generic function's symbol
    Type**  args;        // concrete type arguments
    size_t  arg_count;
    Symbol* inst_sym;    // synthetic symbol for the instantiation (carries its offset)
    ASTNode* inst_decl;  // the cloned+substituted AST_FUNC_DECL
    bool     compiled;
} Instantiation;

static Instantiation* s_insts = NULL;
static size_t s_inst_count = 0, s_inst_cap = 0;
static size_t s_inst_compiled = 0; // drain cursor

// Render a type into a stable string key (used to compare instantiation arg tuples).
static void type_keystr(Type* t, char* out, size_t cap) {
    if (!t) { snprintf(out, cap, "?"); return; }
    switch (t->cls) {
        case TYPE_PRIMITIVE: snprintf(out, cap, "p%d", (int)t->primitive); break;
        case TYPE_POINTER: {
            char inner[128]; type_keystr(t->pointer_base, inner, sizeof(inner));
            snprintf(out, cap, "*%s", inner); break;
        }
        case TYPE_ARRAY: {
            char inner[128]; type_keystr(t->array.element, inner, sizeof(inner));
            snprintf(out, cap, "a%llu_%s", (unsigned long long)t->array.count, inner); break;
        }
        case TYPE_STRUCT: snprintf(out, cap, "s%s", t->struct_name ? t->struct_name : "?"); break;
        case TYPE_PARAM:  snprintf(out, cap, "T%s", t->param_name ? t->param_name : "?"); break;
        case TYPE_FUNCTION: {
            size_t p = snprintf(out, cap, "f%zu", t->function.param_count);
            for (size_t i = 0; i < t->function.param_count; i++) {
                char inner[128]; type_keystr(t->function.param_types[i], inner, sizeof(inner));
                if (p < cap) p += snprintf(out + p, cap - p, "_%s", inner);
            }
            if (t->function.return_type) {
                char inner[128]; type_keystr(t->function.return_type, inner, sizeof(inner));
                if (p < cap) p += snprintf(out + p, cap - p, "_%s", inner);
            }
            break;
        }
        case TYPE_FN_LITERAL:
            // Key by the SYMBOL POINTER (nominal identity), not by name text --
            // this is the field that actually distinguishes `apply(x,y,asc)`
            // from `apply(x,y,desc)` as separate generic instantiations. Two
            // different top-level functions never share a Symbol*, and the
            // same function reused across call sites always resolves to the
            // same Symbol*, so this is both distinguishing and stable, which
            // is exactly what an instantiation cache key needs.
            snprintf(out, cap, "fnlit%p", (void*)t->fn_lit.sym);
            break;
        case TYPE_CONST_VALUE:
            if (t->cval.defer) {
                // deferred (template) value: key by its symbolic ident
                ASTNode* d = t->cval.defer;
                if (d->type == AST_IDENT)
                    snprintf(out, cap, "c$%.*s", (int)d->ident.name_len, d->ident.name);
                else
                    snprintf(out, cap, "c$expr");
            } else if (t->cval.is_agg && t->cval.pin) {
                // aggregate value: key by the actual bytes so distinct aggregates
                // instantiate distinctly (not by the union-aliased scalar field).
                uint64_t sz = Type_SizeOf(t->cval.pin);
                if (sz > 64) sz = 64; // bound the key; collisions beyond 64B are astronomically unlikely here
                uint8_t buf[64];
                size_t p = 0;
                p += snprintf(out + p, cap - p, "cA");
                if (ConstEval_ReadBytes(t->cval.agg_off, buf, sz)) {
                    for (uint64_t k = 0; k < sz && p + 2 < cap; k++)
                        p += snprintf(out + p, cap - p, "%02x", buf[k]);
                }
            } else {
                snprintf(out, cap, "c%lld", (long long)t->cval.scalar);
            }
            break;
        default: snprintf(out, cap, "?"); break;
    }
}

static bool type_args_equal(Type** a, Type** b, size_t n) {
    char ka[256], kb[256];
    for (size_t i = 0; i < n; i++) {
        type_keystr(a[i], ka, sizeof(ka));
        type_keystr(b[i], kb, sizeof(kb));
        if (strcmp(ka, kb) != 0) return false;
    }
    return true;
}



// --- deep AST clone with type substitution applied to every embedded Type* ---
// Non-static: shared with backendllvm.c (generic instantiation uses the same clone).
ASTNode* clone_ast(ASTNode* n, const char** params, Type** args, size_t np, bool clone_symbols);

static Symbol** s_clone_map_old = NULL;
static Symbol** s_clone_map_new = NULL;
static size_t s_clone_map_count = 0;
static size_t s_clone_map_cap = 0;

// Aggregate value-param materialization for the CURRENT instantiation.
// A value param pinned to a struct/array (e.g. `[Cfg C]`) can't be flattened to
// an int literal the way a scalar `[u32 N]` is — its uses are field/index reads
// (C.w). Instead we materialize its constexpr-folded bytes into a synthetic
// SYM_GLOBAL of the pin type (reusing the exact const-aggregate-global pipeline:
// global_bytes + data-section slot + Backend_SetGlobalBytes at emit), and rewrite
// each `C` ident to reference that global. Keyed by param NAME for this instance;
// reset per instantiation so distinct instances get distinct globals.
static const char* s_agg_param_names[16];
static Symbol*     s_agg_param_syms[16];
static size_t      s_agg_param_count = 0;
static int         s_agg_counter = 0; // unique-name suffix across all instantiations

// Look up a materialized aggregate value-param by name (NULL if not one).
static Symbol* agg_param_sym(const char* name, size_t len) {
    for (size_t i = 0; i < s_agg_param_count; i++)
        if (strlen(s_agg_param_names[i]) == len &&
            strncmp(s_agg_param_names[i], name, len) == 0)
            return s_agg_param_syms[i];
    return NULL;
}

// Materialize one aggregate value-arg into a synthetic const global and record it
// under the param name. `pin` is the aggregate type; `agg_off` its arena offset.
static void materialize_agg_param(const char* pname, Type* pin, uint32_t agg_off, ASTNode* err_node) {
    if (s_agg_param_count >= 16) return; // more than 16 aggregate params: give up quietly
    uint64_t sz = Type_SizeOf(pin);
    uint8_t* bytes = (uint8_t*)calloc(1, sz ? sz : 1);
    if (!ConstEval_ReadBytes(agg_off, bytes, sz)) { free(bytes); return; }
    // Same escape-boundary rule an ordinary `const Agg X = {...}` decl already
    // enforces (parser.c's parse_const_decl) -- a real bug found via a segfault:
    // a struct-typed const-generic VALUE holding a function-pointer field (e.g.
    // `struct Cfg{ fn(i32)i32 op }` used as `Wrapped[T, Cfg C]`) reached here
    // uncaught, and these raw bytes (a Symbol* -- the compiler's own
    // process-space heap pointer, meaningless at runtime) got copied verbatim
    // into the emitted binary's data section. Calling through `C.op` at
    // runtime then jumped to a stale compiler-process address. This wasn't
    // wired to the escape check at all before; now it is, turning the crash
    // into the same clean, honest compile error a plain pointer field already gets.
    if (ConstEval_AggHasEscapingPtr(pin, bytes, sz)) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "const-generic value for '%s' stores a pointer or function reference from "
                 "compile-time memory that has no runtime address (a comptime-heap pointer/"
                 "fn-symbol can't escape into a const-generic argument); store a value or an "
                 "index, not a pointer", pname);
        Error_AtNode(err_node, msg, NULL);
    }
    // Synthetic name that the lexer can never produce ($ prefix), unique per use.
    char nbuf[64];
    int n = snprintf(nbuf, sizeof(nbuf), "$cgen$%s$%d", pname, s_agg_counter++);
    Symbol* g = SymTable_Add(Get_SymTable(), nbuf, (size_t)n, pin, SYM_GLOBAL);
    g->global_bytes = bytes;
    g->has_init = true;
    // Register it for emission like any other global with an initializer.
    //
    // Everything else that reaches the emit list does so from parse_global_decl
    // in parser.c, because every OTHER global appears literally in the source.
    // This one is synthesized here, during typecheck, so nothing had registered
    // it -- and a backend that emits only what the registry lists (the LLVM one)
    // therefore REFERENCED `@$cgen$C$0` from the method body without ever
    // defining it: "use of undefined value". The x86-64 backend resolved the
    // symbol directly and so never noticed.
    Global_RegisterForEmit(g);
    s_agg_param_names[s_agg_param_count] = pname;
    s_agg_param_syms[s_agg_param_count]  = g;
    s_agg_param_count++;
}

static Symbol* clone_symbol(Symbol* s, const char** params, Type** args, size_t np, bool clone_symbols) {
    if (!s) return NULL;
    if (!clone_symbols) {
        s->type = Type_Substitute(s->type, params, args, np);
        return s;
    }
    if (!s) return NULL;
    for (size_t i = 0; i < s_clone_map_count; i++) {
        if (s_clone_map_old[i] == s) return s_clone_map_new[i];
    }
    
    Symbol* c = (Symbol*)calloc(1, sizeof(Symbol));
    *c = *s;
    
    if (s_clone_map_count >= s_clone_map_cap) {
        s_clone_map_cap = s_clone_map_cap ? s_clone_map_cap * 2 : 32;
        s_clone_map_old = realloc(s_clone_map_old, s_clone_map_cap * sizeof(Symbol*));
        s_clone_map_new = realloc(s_clone_map_new, s_clone_map_cap * sizeof(Symbol*));
    }
    s_clone_map_old[s_clone_map_count] = s;
    s_clone_map_new[s_clone_map_count] = c;
    s_clone_map_count++;
    
    c->type = Type_Substitute(s->type, params, args, np);
    return c;
}

ASTNode* clone_ast(ASTNode* n, const char** params, Type** args, size_t np, bool clone_symbols) {
    if (!n) return NULL;
    ASTNode* c = (ASTNode*)calloc(1, sizeof(ASTNode));
    *c = *n; // shallow copy first; then deep-copy children + substitute types
    c->result_type = NULL; // force re-inference under substituted types

    switch (n->type) {
        case AST_INT_LITERAL: case AST_STRING:
            break;
        case AST_SIZEOF:
        case AST_ALIGNOF:
            c->sizeof_expr.type = Type_Substitute(n->sizeof_expr.type, params, args, np);
            break;
        case AST_OFFSETOF: {
            Type* st = Type_Substitute(n->field_ref_expr.type, params, args, np);
            c->field_ref_expr.type = st;
            c->field_ref_expr.index_expr = clone_ast(n->field_ref_expr.index_expr, params, args, np, clone_symbols);
            break;
        }
        case AST_NAMEOF: {
            Type* st = Type_Substitute(n->field_ref_expr.type, params, args, np);
            c->field_ref_expr.type = st;
            c->field_ref_expr.index_expr = clone_ast(n->field_ref_expr.index_expr, params, args, np, clone_symbols);
            break;
        }
        case AST_TYPE_EXPR:
            // Same field, same substitution as AST_SIZEOF right above — a bare
            // type-param reference used as an expression (T == i32) must become
            // fully concrete at monomorphization time, exactly like every other
            // node that mentions a generic param. Without this, T stayed an
            // abstract TYPE_PARAM reference all the way to codegen, which has
            // no case for it at all (unlike AST_SIZEOF, which always folds away
            // via ConstEval before codegen, or has its own real codegen case
            // for the few situations where it can't) — that unconcretized node
            // reaching compile_node_ctx was the actual cause of a real crash.
            c->sizeof_expr.type = Type_Substitute(n->sizeof_expr.type, params, args, np);
            break;
        case AST_IDENT: {
            // Check if this ident refers to a const generic parameter
            bool is_const_param = false;
            if (params && args) {
                for (size_t i = 0; i < np; i++) {
                    if (strlen(params[i]) == n->ident.name_len &&
                        strncmp(n->ident.name, params[i], n->ident.name_len) == 0) {
                        if (args[i] && args[i]->cls == TYPE_CONST_VALUE) {
                            // Aggregate value-param: reference its materialized const
                            // global (typed by the pin) so field/index access resolves.
                            Symbol* g = agg_param_sym(params[i], strlen(params[i]));
                            if (g) {
                                c->ident.sym = g;
                                c->ident.name = g->name;
                            c->ident.name_len = strlen(g->name);
                            is_const_param = true;
                            break;
                        }
                        // Scalar value-param: previously flattened straight to a
                        // bare int literal, which silently discarded the pin
                        // (u8, u16, ...) — Type_Infer on a bare literal defaults
                        // to i32/i64 by its own value-fits rule, unrelated to
                        // the param's declared width, so e.g. `N + 250` with
                        // `u8 N=10` inferred as i32 instead of u8 and never
                        // wrapped at 8 bits. Wrap in a cast to the pin instead:
                        // reuses the AST_CAST codegen/type-inference path that
                        // already narrows correctly, rather than inventing a
                        // new "literal with a declared type" concept.
                        //
                        // Float pins (f32/f64) need a genuinely different
                        // literal shape: ConstEval stores a folded float's
                        // BIT PATTERN in the same int64_t scalar field (see
                        // ce_f_to_bits in constexpr.c) — it is not a plain
                        // integer value that happens to need narrowing. This
                        // was previously always built as AST_INT_LITERAL with
                        // lit_kind=LIT_INT regardless of pin, which fed the
                        // float's raw bit pattern into the cast machinery AS
                        // IF it were an ordinary integer to truncate — e.g.
                        // `struct Scaled[f32 K]` instantiated as `Scaled[2.5]`
                        // silently produced 0 instead of 2.5, since 2.5's f64
                        // bit pattern reinterpreted as a huge integer and cast
                        // down to f32 has no meaningful relationship to 2.5.
                        // Build a LIT_FLOAT literal instead, with the bits
                        // correctly reinterpreted back to a double first (the
                        // same memcpy-based reinterpretation ConstEval itself
                        // uses, not a numeric int-to-float conversion).
                        {
                            ASTNode* lit = (ASTNode*)calloc(1, sizeof(ASTNode));
                            Type* pin = args[i]->cval.pin;
                            if (pin && Type_IsFloat(pin)) {
                                double d;
                                int64_t bits = args[i]->const_val;
                                memcpy(&d, &bits, sizeof(d));
                                lit->type = AST_INT_LITERAL;
                                lit->lit_kind = LIT_FLOAT;
                                lit->float_value = d;
                            } else {
                                lit->type = AST_INT_LITERAL;
                                lit->int_value = args[i]->const_val;
                                // A fn-typed const generic param (`fn(T,T) T Op`)
                                // was folded (constexpr.c, AST_IDENT/SYM_FUNCTION
                                // case) to the bare Symbol* of the chosen function,
                                // not a real integer. Mark it so the backend never
                                // treats those bits as an ordinary constant.
                                lit->lit_kind = (pin && pin->cls == TYPE_FUNCTION)
                                    ? LIT_FN_SYMBOL : LIT_INT;
                            }
                            c->type = AST_CAST;
                            c->cast.target_type = pin ? pin : Type_MakePrim(PRIM_I32);
                            c->cast.expr = lit;
                        }
                        is_const_param = true;
                        break;
                    }
                }
            }
            }
            if (is_const_param) break;

            // An ident carrying EXPLICIT generic type args (`thunk[T]` in value position)
            // holds Type*s like any other node that mentions a generic param -- and like
            // AST_SIZEOF / AST_CAST / AST_NEW / AST_TYPE_EXPR above, they must be made
            // concrete HERE, or the instantiation is keyed on an abstract TYPE_PARAM.
            //
            // Without this, `thunk[T]` inside a generic instantiated `thunk` with T still
            // abstract; its body's `T* o = (T*)p  o.draw()` then did a field access on a
            // non-struct and died with a nonsense error. Concrete args (`thunk[Circle]`)
            // happened to work only because there was nothing to substitute -- which is
            // exactly why the concrete case passed and the useful one didn't.
            //
            // This is what makes a thunk usable from inside a generic packer, i.e. what
            // makes it useful at all:
            //     fn pack[T](T* o) Dyn[VT] { return dyn(o, thunk[T]) }
            if (n->ident.type_arg_count > 0 && n->ident.type_args) {
                size_t tc = n->ident.type_arg_count;
                Type** sub = (Type**)malloc(tc * sizeof(Type*));
                for (size_t i = 0; i < tc; i++)
                    sub[i] = Type_Substitute(n->ident.type_args[i], params, args, np);
                c->ident.type_args = sub;
                c->ident.type_arg_count = tc;
            }

            // Clone the symbol with substituted types so field accesses on
            // this ident resolve against the concrete (instantiated) struct.
            if (n->ident.sym)
                c->ident.sym = clone_symbol(n->ident.sym, params, args, np, clone_symbols);
            break;
        }
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR: case AST_SHL: case AST_SHR:
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_ASSIGN:
            c->binary.left = clone_ast(n->binary.left, params, args, np, clone_symbols);
            c->binary.right = clone_ast(n->binary.right, params, args, np, clone_symbols);
            break;
        case AST_LOGICAL_NOT: case AST_BIT_NOT: case AST_DEREF: case AST_ADDR: case AST_RETURN:
            c->unary = clone_ast(n->unary, params, args, np, clone_symbols);
            break;
        case AST_CAST:
            c->cast.target_type = Type_Substitute(n->cast.target_type, params, args, np);
            c->cast.expr = clone_ast(n->cast.expr, params, args, np, clone_symbols);
            break;
        case AST_DECLARATION: {
            c->decl.var_type = Type_Substitute(n->decl.var_type, params, args, np);
            c->decl.init_expr = clone_ast(n->decl.init_expr, params, args, np, clone_symbols);
            c->decl.sym = clone_symbol(n->decl.sym, params, args, np, clone_symbols);
            // A `const` deferred from parse time because its initializer
            // mentioned a generic param (see parser.c parse_const_decl). Now
            // that this clone belongs to one concrete instantiation, params/args
            // hold real values, so fold it the same way Type_Substitute folds a
            // deferred TYPE_CONST_VALUE (types.c): same frame convention, same
            // ConstEval call, just applied to a statement's init_expr instead of
            // a type's count_expr. Bake the result in as a literal so any later
            // use of this name within the clone (e.g. as an array size) sees a
            // concrete, per-instantiation value instead of a runtime expr.
            if (n->decl.is_generic_const) {
                CeGenericFrame saved = ce_generic_frame_install(params, args, np);
                int64_t v;
                s_ce_isfloat = false;
                s_ce_isfnsym = false;
                bool ok = ConstEval(c->decl.init_expr, &v);
                bool was_float = s_ce_isfloat;
                bool was_fnsym = s_ce_isfnsym;
                ce_generic_frame_restore(saved);
                if (!ok) {
                    fprintf(stderr, "Error: const '%.*s' initializer is not a constant "
                            "expression for this instantiation\n",
                            (int)n->decl.name_len, n->decl.name);
                } else {
                    ASTNode* lit = (ASTNode*)calloc(1, sizeof(ASTNode));
                    lit->type = AST_INT_LITERAL;
                    // Same reasoning as AST_CONST_EXPR just below: a fn-typed
                    // generic const folds to a Symbol* (constexpr.c), not a
                    // real integer -- tag it so the backend resolves the
                    // function's real address instead of moving the raw
                    // pointer bits. (Note: this path doesn't yet handle
                    // was_float the way AST_CONST_EXPR does -- pre-existing
                    // gap, out of scope for this fix.)
                    if (was_fnsym) {
                        lit->lit_kind = LIT_FN_SYMBOL;
                        lit->int_value = (uint64_t)v;
                    } else {
                        lit->int_value = v;
                    }
                    lit->result_type = c->decl.var_type;
                    c->decl.init_expr = lit;
                }
            }
            break;
        }
        case AST_CONST_EXPR: {
            // const(EXPR) inside a generic body: the fold was deferred at parse time
            // because EXPR mentions an in-scope generic param, and the template is
            // parsed once, before any instantiation exists. Now that this clone
            // belongs to ONE concrete instantiation, fold it -- same frame
            // convention and same ConstEval call the deferred `is_generic_const`
            // declaration path above uses, just yielding an expression instead of a
            // statement's initializer.
            //
            // NOTE this replaces the AST_CONST_EXPR node with the folded literal, so
            // nothing downstream ever sees this node kind -- const(...) leaves no
            // trace in the tree and emits no code, exactly like a scalar const.
            CeGenericFrame saved = ce_generic_frame_install(params, args, np);
            int64_t v = 0;
            s_ce_isfloat = false;
            s_ce_isfnsym = false;
            bool ok = ConstEval(n->const_expr.inner, &v);
            bool was_float = s_ce_isfloat;
            bool was_fnsym = s_ce_isfnsym;
            ce_generic_frame_restore(saved);
            if (!ok) {
                fprintf(stderr, "Error: const(...) operand is not a constant "
                                "expression for this instantiation\n");
                c->type = AST_INT_LITERAL;
                c->lit_kind = LIT_INT;
                c->int_value = 0;
            } else {
                // Rewrite this node in place as the folded literal. Float-ness must
                // be preserved: a comptime float travels as IEEE-754 bits in the
                // int64 slot, so emit LIT_FLOAT (bitcast back) rather than an
                // integer that happens to hold that bit pattern. Same reasoning
                // for fn-symbol-ness: const(Op) on a fn-typed const generic
                // param folds to a Symbol* (constexpr.c), not a real integer —
                // emit LIT_FN_SYMBOL so the backend resolves it to the
                // function's real address instead of moving the raw pointer.
                c->type = AST_INT_LITERAL;
                if (was_float) {
                    c->lit_kind = LIT_FLOAT;
                    double d; memcpy(&d, &v, sizeof d);
                    c->float_value = d;
                } else if (was_fnsym) {
                    c->lit_kind = LIT_FN_SYMBOL;
                    c->int_value = (uint64_t)v;
                } else {
                    c->lit_kind = LIT_INT;
                    c->int_value = (uint64_t)v;
                }
            }
            break;
        }
        case AST_BLOCK: {
            c->block.statements = malloc(n->block.count * sizeof(ASTNode*));
            c->block.capacity = n->block.count;
            for (size_t i = 0; i < n->block.count; i++)
                c->block.statements[i] = clone_ast(n->block.statements[i], params, args, np, clone_symbols);
            break;
        }
        case AST_IF:
            c->if_stmt.condition = clone_ast(n->if_stmt.condition, params, args, np, clone_symbols);
            c->if_stmt.true_block = clone_ast(n->if_stmt.true_block, params, args, np, clone_symbols);
            c->if_stmt.false_block = clone_ast(n->if_stmt.false_block, params, args, np, clone_symbols);
            // `match T` arm: substitute the scrutinee and pattern types so that at
            // this instantiation the scrutinee (e.g. T) becomes concrete and branch
            // selection (types.c AST_IF reflect path) can unify. The pattern is also
            // substituted, though its OWN wildcards are not in this frame's params —
            // Type_Substitute leaves an unmatched TYPE_PARAM untouched, so pattern
            // holes survive substitution and get bound by reflect_unify instead.
            c->if_stmt.reflect_scrutinee = Type_Substitute(n->if_stmt.reflect_scrutinee, params, args, np);
            c->if_stmt.reflect_pattern   = Type_Substitute(n->if_stmt.reflect_pattern, params, args, np);
            break;
        case AST_WHILE:
            c->while_stmt.condition = clone_ast(n->while_stmt.condition, params, args, np, clone_symbols);
            c->while_stmt.body = clone_ast(n->while_stmt.body, params, args, np, clone_symbols);
            break;
        case AST_FOR:
            c->for_stmt.init = clone_ast(n->for_stmt.init, params, args, np, clone_symbols);
            c->for_stmt.cond = clone_ast(n->for_stmt.cond, params, args, np, clone_symbols);
            c->for_stmt.incr = clone_ast(n->for_stmt.incr, params, args, np, clone_symbols);
            c->for_stmt.body = clone_ast(n->for_stmt.body, params, args, np, clone_symbols);
            break;
        case AST_CALL: {
            c->call.args = malloc((n->call.arg_count ? n->call.arg_count : 1) * sizeof(ASTNode*));
            for (size_t i = 0; i < n->call.arg_count; i++)
                c->call.args[i] = clone_ast(n->call.args[i], params, args, np, clone_symbols);
            c->call.target_expr = clone_ast(n->call.target_expr, params, args, np, clone_symbols);
            if (n->call.type_arg_count) {
                c->call.type_args = malloc(n->call.type_arg_count * sizeof(Type*));
                for (size_t i = 0; i < n->call.type_arg_count; i++)
                    c->call.type_args[i] = Type_Substitute(n->call.type_args[i], params, args, np);
            }
            break;
        }
        case AST_FUNC_DECL: {
            c->func_decl.return_type = Type_Substitute(n->func_decl.return_type, params, args, np);
            c->func_decl.param_syms = (Symbol**)calloc(n->func_decl.param_count, sizeof(Symbol*));
            for (size_t i = 0; i < n->func_decl.param_count; i++)
                c->func_decl.param_syms[i] = clone_symbol(n->func_decl.param_syms[i], params, args, np, clone_symbols);
            c->func_decl.body = clone_ast(n->func_decl.body, params, args, np, clone_symbols);
            c->func_decl.type_params = NULL; // the clone is concrete, not generic
            c->func_decl.type_param_count = 0;
            break;
        }
        case AST_NEW:
            c->new_expr.alloc_type = Type_Substitute(n->new_expr.alloc_type, params, args, np);
            c->new_expr.init = clone_ast(n->new_expr.init, params, args, np, clone_symbols);
            c->new_expr.count = clone_ast(n->new_expr.count, params, args, np, clone_symbols);
            break;
        case AST_DELETE:
            c->delete_expr.ptr = clone_ast(n->delete_expr.ptr, params, args, np, clone_symbols);
            break;
        case AST_FIELD:
            c->field.base = clone_ast(n->field.base, params, args, np, clone_symbols);
            c->field.field_name = n->field.field_name;
            c->field.field_name_len = n->field.field_name_len;
            // Clear cached field/sdef so Type_Infer re-resolves against
            // the concrete (substituted) struct, not the generic template.
            c->field.field = NULL;
            c->field.sdef = NULL;
            break;
        case AST_INDEX:
            c->index.base = clone_ast(n->index.base, params, args, np, clone_symbols);
            c->index.index = clone_ast(n->index.index, params, args, np, clone_symbols);
            break;
        case AST_STRUCT_LITERAL: {
            c->struct_lit.values = malloc(n->struct_lit.count * sizeof(ASTNode*));
            for (size_t i = 0; i < n->struct_lit.count; i++)
                c->struct_lit.values[i] = clone_ast(n->struct_lit.values[i], params, args, np, clone_symbols);
            // Substitute the struct def if it's an instantiated generic (e.g. Option$TT -> Option$p2)
            if (n->struct_lit.sdef && n->struct_lit.sdef->generic_base) {
                Type** new_args = malloc(n->struct_lit.sdef->type_arg_count * sizeof(Type*));
                for (size_t i = 0; i < n->struct_lit.sdef->type_arg_count; i++)
                    new_args[i] = Type_Substitute(n->struct_lit.sdef->type_args[i], params, args, np);
                c->struct_lit.sdef = Struct_Instantiate(n->struct_lit.sdef->generic_base, new_args, n->struct_lit.sdef->type_arg_count);
                free(new_args);
            }
            break;
        }
        case AST_ARRAY_LITERAL: {
            c->array_lit.values = malloc(n->array_lit.count * sizeof(ASTNode*));
            for (size_t i = 0; i < n->array_lit.count; i++)
                c->array_lit.values[i] = clone_ast(n->array_lit.values[i], params, args, np, clone_symbols);
            c->array_lit.elem_type = Type_Substitute(n->array_lit.elem_type, params, args, np);
            break;
        }
        default: break;
    }
    return c;
}

// Resolve every type-level `match T { pattern { ... } ... }` in an
// already-monomorphized tree, once, right after clone_ast. `match T` lowers
// to a chain of AST_IF nodes carrying a reflect_pattern; clone_ast has just
// substituted both reflect_scrutinee and reflect_pattern for this
// instantiation (see clone_ast's own AST_IF case above), so the scrutinee is
// now concrete and the branch decision can be made for good, right here.
//
// This used to be duplicated: Typecheck_Tree resolved it lazily (only the
// taken arm, on demand, the first time control flow reached that AST_IF),
// and ConstEval needed its own copy of the exact same reflect_unify/clone_ast
// dance because it walks generic-instantiated bodies that skip
// Typecheck_Tree entirely. Neither evaluator has any business knowing type
// reflection exists — `match T` is a compile-time AST rewrite (a monomorphization
// step), not a value-level operation, so it belongs at the one seam where a
// generic body becomes concrete: right here, in Generic_Instantiate, before
// ANY consumer (typecheck, constexpr, or a future codegen/LLVM backend) ever
// sees the body. After this call, the tree contains no reflect_pattern nodes
// left at all — every consumer downstream can go back to being a plain
// AST interpreter/lowerer with zero awareness of type reflection.
//
// Eager rather than lazy: every AST_IF in the whole body is resolved up
// front, not only the ones control flow happens to reach. This is safe
// because `match T`'s branch decision depends only on T (already fixed for
// the whole instantiation), never on runtime control flow — an arm's
// type-correctness can't depend on whether it's reached. Mirrors clone_ast's
// own traversal exactly (same node kinds, same child fields) so nothing
// under a func decl / block / control-flow node is left unvisited.
static void Resolve_Reflect_Matches(ASTNode* n) {
    if (!n) return;
    switch (n->type) {
        case AST_IF:
            // Shared core with Typecheck_Tree's identical AST_IF case -- see
            // reflect_match_select. This copy runs eagerly right after
            // clone_ast, so its own follow-up is just recursing into itself
            // (the taken/fallback block may hold further nested matches).
            if (n->if_stmt.reflect_pattern) {
                if (reflect_match_select(n)) Resolve_Reflect_Matches(n);
                return;
            }
            Resolve_Reflect_Matches(n->if_stmt.condition);
            Resolve_Reflect_Matches(n->if_stmt.true_block);
            Resolve_Reflect_Matches(n->if_stmt.false_block);
            return;
        case AST_BLOCK:
            for (size_t i = 0; i < n->block.count; i++)
                Resolve_Reflect_Matches(n->block.statements[i]);
            return;
        case AST_WHILE:
            Resolve_Reflect_Matches(n->while_stmt.condition);
            Resolve_Reflect_Matches(n->while_stmt.body);
            return;
        case AST_FOR:
            Resolve_Reflect_Matches(n->for_stmt.init);
            Resolve_Reflect_Matches(n->for_stmt.cond);
            Resolve_Reflect_Matches(n->for_stmt.incr);
            Resolve_Reflect_Matches(n->for_stmt.body);
            return;
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR: case AST_SHL: case AST_SHR:
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_ASSIGN:
            Resolve_Reflect_Matches(n->binary.left);
            Resolve_Reflect_Matches(n->binary.right);
            return;
        case AST_LOGICAL_NOT: case AST_BIT_NOT: case AST_DEREF: case AST_ADDR: case AST_RETURN:
            Resolve_Reflect_Matches(n->unary);
            return;
        case AST_CAST:
            Resolve_Reflect_Matches(n->cast.expr);
            return;
        case AST_DECLARATION:
            Resolve_Reflect_Matches(n->decl.init_expr);
            return;
        case AST_CALL:
            for (size_t i = 0; i < n->call.arg_count; i++)
                Resolve_Reflect_Matches(n->call.args[i]);
            Resolve_Reflect_Matches(n->call.target_expr);
            return;
        case AST_FUNC_DECL:
            Resolve_Reflect_Matches(n->func_decl.body);
            return;
        case AST_NEW:
            Resolve_Reflect_Matches(n->new_expr.init);
            Resolve_Reflect_Matches(n->new_expr.count);
            return;
        case AST_DELETE:
            Resolve_Reflect_Matches(n->delete_expr.ptr);
            return;
        case AST_FIELD:
            Resolve_Reflect_Matches(n->field.base);
            return;
        case AST_INDEX:
            Resolve_Reflect_Matches(n->index.base);
            Resolve_Reflect_Matches(n->index.index);
            return;
        case AST_STRUCT_LITERAL:
            for (size_t i = 0; i < n->struct_lit.count; i++)
                Resolve_Reflect_Matches(n->struct_lit.values[i]);
            return;
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < n->array_lit.count; i++)
                Resolve_Reflect_Matches(n->array_lit.values[i]);
            return;
        case AST_OFFSETOF:
        case AST_NAMEOF: {
            Type* st = n->field_ref_expr.type;
            if (st == NULL || st->cls != TYPE_PARAM) {
                if (n->field_ref_expr.index_expr == NULL) {
                    if (n->type == AST_NAMEOF) {
                        n->type = AST_STRING;
                        char buf[256];
                        Type_ToString(st, buf, sizeof(buf));
                        n->int_value = (uint64_t)(uintptr_t)strdup(buf);
                    }
                    return;
                }
                int64_t idx;
                // index_expr is already fully concrete thanks to clone_ast substituting
                // scalar generic params with AST_INT_LITERALs.
                bool ok = st->cls == TYPE_STRUCT && ConstEval(n->field_ref_expr.index_expr, &idx);
                if (ok) {
                    StructDef* sd = Struct_Find(st->struct_name);
                    if (sd) Struct_Layout(sd);
                    if (!sd || idx < 0 || (uint64_t)idx >= sd->field_count) {
                        char errbuf[128];
                        snprintf(errbuf, sizeof(errbuf),
                                "%s: field index out of range for this instantiation",
                                n->type == AST_NAMEOF ? "nameof" : "offsetof");
                        Error_AtNode(n, errbuf, NULL);
                    }
                    if (n->type == AST_NAMEOF) {
                        n->type = AST_STRING;
                        n->int_value = (uint64_t)(uintptr_t)strdup(sd->fields[idx].name);
                    } else {
                        n->type = AST_INT_LITERAL;
                        n->lit_kind = LIT_INT;
                        n->int_value = sd->fields[idx].offset;
                    }
                }
                return;
            }
            Resolve_Reflect_Matches(n->field_ref_expr.index_expr);
            return;
        }
        default:
            return; // literals, idents, sizeof, type exprs: no reflect_pattern children
    }
}

// Find or create the instantiation for (generic, type args); enqueue if new.
// Returns the synthetic symbol whose offset the call fixup should target.
// Monomorphize once, cache forever: clone+substitute is pure AST work
// (clone_ast has no codegen dependency), so this function has none either
// despite living in this file — it's exposed publicly (compiler.h) so
// constexpr.c's comptime call path can reuse the exact same instantiation
// and cache as the JIT/AOT path, instead of needing its own copy.
Symbol* Generic_Instantiate(Symbol* generic, Type** targs, size_t targ_count) {
    for (size_t i = 0; i < s_inst_count; i++) {
        if (s_insts[i].generic == generic &&
            s_insts[i].arg_count == targ_count &&
            type_args_equal(s_insts[i].args, targs, targ_count)) {
            return s_insts[i].inst_sym; // cache hit
        }
    }
    // New instantiation: clone+substitute the generic decl now; compile later.
    ASTNode* gdecl = generic->generic_decl;
    const char** params = gdecl->func_decl.type_params;
    size_t np = gdecl->func_decl.type_param_count;
    if (targ_count != np) {
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf),
                "generic '%.*s' expects %zu type arguments, got %zu",
                (int)gdecl->func_decl.name_len, gdecl->func_decl.name, np, targ_count);
        Error_AtNode(gdecl, errbuf, NULL);
    }
    // Deep-copy the arg types so the cache owns stable copies.
    Type** argcopy = malloc(targ_count * sizeof(Type*));
    for (size_t i = 0; i < targ_count; i++) argcopy[i] = targs[i];

    s_clone_map_count = 0; // reset the symbol identity map for this new instantiation
    // Materialize any aggregate value-params (pinned to struct/array) into synthetic
    // const globals so `C.w`-style uses in the body resolve. Scalar value-params are
    // handled inline in clone_ast (flattened to an int literal). Reset per instance.
    s_agg_param_count = 0;
    for (size_t i = 0; i < np; i++) {
        Type* a = argcopy[i];
        if (a && a->cls == TYPE_CONST_VALUE && a->cval.is_agg && Type_IsAggregate(a->cval.pin)) {
            materialize_agg_param(params[i], a->cval.pin, a->cval.agg_off, gdecl);
        }
    }
    ASTNode* inst = clone_ast(gdecl, params, argcopy, np, true);
    Resolve_Reflect_Matches(inst); // resolve every `match T` in this instantiation
                                    // once, here, so no downstream consumer
                                    // (Typecheck_Tree, ConstEval, or a future
                                    // codegen/LLVM lowering) ever sees one.
    Symbol* isym = (Symbol*)calloc(1, sizeof(Symbol));
    isym->kind = SYM_FUNCTION;
    isym->type = Type_Substitute(generic->type, params, argcopy, np);
    isym->name = gdecl->func_decl.name;
    isym->name_len = gdecl->func_decl.name_len;
    isym->func_decl = inst; // the monomorphized body — needed by constexpr.c's
                             // AST-walking interpreter, which has no use for a
                             // compiled offset the way the JIT call site does.
    inst->func_decl.sym = isym;

    if (s_inst_count >= s_inst_cap) {
        s_inst_cap = s_inst_cap ? s_inst_cap * 2 : 16;
        s_insts = realloc(s_insts, s_inst_cap * sizeof(Instantiation));
    }
    s_insts[s_inst_count].generic = generic;
    s_insts[s_inst_count].args = argcopy;
    s_insts[s_inst_count].arg_count = targ_count;
    s_insts[s_inst_count].inst_sym = isym;
    s_insts[s_inst_count].inst_decl = inst;
    s_insts[s_inst_count].compiled = false;
    s_inst_count++;
    return isym;
}


size_t Generic_GetCount(void) {
    return s_inst_count;
}

ASTNode* Generic_GetDecl(size_t index) {
    if (index >= s_inst_count) return NULL;
    return s_insts[index].inst_decl;
}

void Typecheck_Program(ASTNode** units, size_t count) {
    for (size_t i = 0; i < count; i++) {
        Typecheck_Tree(units[i]);
    }
    
    // Drain the generic-instantiation queue.
    // Compiling/typechecking an instantiation may itself enqueue further instantiations.
    int guard = 0;
    while (s_inst_compiled < s_inst_count) {
        if (++guard > 100000) {
            fprintf(stderr, "Error: runaway generic instantiation (recursive without base case?)\\n");
            exit(1);
        }
        size_t idx = s_inst_compiled++;
        if (s_insts[idx].compiled) continue;
        ASTNode* inst_decl = s_insts[idx].inst_decl;

        Typecheck_Tree(inst_decl); // type the substituted clone — may itself
                                    // enqueue further instantiations and
                                    // `realloc` s_insts, so no pointer into
                                    // the array may be held live across this
                                    // call; re-index afterward instead.
        s_insts[idx].compiled = true; // Mark as typechecked
    }
}

bool base_is_lvalue(ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case AST_IDENT:  return true;
        case AST_DEREF:  return true;
        case AST_FIELD:  return base_is_lvalue(node->field.base) ||
                                (Type_Infer(node->field.base) &&
                                 Type_Infer(node->field.base)->cls == TYPE_POINTER);
        case AST_INDEX:  return true; // a[i] is always addressable (array or ptr base)
        // [PLACE-RETURN] A scalar assignment is an addressable place: it writes
        // to (and yields the address of) its destination. `&(a=b)` and further
        // access through the result are lvalues, mirroring compile_lvalue's
        // AST_ASSIGN case.
        case AST_ASSIGN: return base_is_lvalue(node->binary.left);
        default:         return false;
    }
}
