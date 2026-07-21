// reflections.c — structural type unification for `match T` pattern destructuring.
//
// This is the engine behind type-level pattern matching:
//
//     match T {
//         P*    { /* if T is u32*, P binds to u32 here          */ }
//         E[N]  { /* if T is f32[4], E binds to f32, N binds 4  */ }
//         fn(A)B{ /* function types: A, B bind to arg/return    */ }
//     }
//
// A `match T` arm whose pattern contains an *undeclared* identifier registers
// that identifier as a fresh generic param (a TYPE_PARAM for a bare name, a
// value param for an array-size position — see parser.c's in_match_pattern
// path). Those params are the "wildcards": holes in the pattern's shape that
// this file fills in by walking the concrete scrutinee type alongside the
// pattern and reading off whatever the concrete side has where the pattern has
// a hole.
//
// The design deliberately reuses the machinery that already exists for generic
// instantiation rather than inventing a parallel one:
//
//   * A bound wildcard is a plain TYPE_PARAM whose name matches a pattern hole.
//     We don't mutate it in place; we collect (name -> concrete Type*) bindings
//     into a ReflectBindings list, exactly the shape clone_ast/Type_Substitute
//     already consume for monomorphization. Selecting a match arm then substitutes
//     the arm body against those bindings — the same transform a generic call
//     applies to a function body. So "P is u32 in this block" and "T is u32 in
//     this instantiation" go through one code path, not two.
//
//   * An array-size hole (`N` in `E[N]`) binds to a TYPE_CONST_VALUE carrying the
//     concrete array's count as a scalar pinned to u32 — the identical
//     representation const generics already use for value params. `N` then flows
//     into array sizes / arithmetic / method bodies in the arm with no
//     special-casing.
//

// reflect_unify returns false on a structural-shape mismatch (pointer vs array,
// different primitive, different struct name, arity mismatch), which is what
// drives dead-branch elimination: the arm whose pattern does NOT unify with the
// concrete scrutinee is dropped, unread, before typecheck ever looks inside it.

#include "compiler.h"
#include <string.h>
#include <stdlib.h>

// A bare identifier registered as a wildcard is stamped TYPE_PARAM by the parser
// (parser.c, in_match_pattern path). A pattern hole is therefore recognized purely
// structurally: it's a TYPE_PARAM node. There is no separate "wildcard" flag — a
// TYPE_PARAM in a match pattern IS the hole, by construction, which keeps this in
// lockstep with how every other part of the compiler already recognizes a param.
static bool is_hole(const Type* pat) {
    return pat && pat->cls == TYPE_PARAM && pat->param_name != NULL;
}

// Record binding `name -> concrete`, or, if `name` is already bound, require the
// new binding to agree with the old one. The agreement check is what makes a
// pattern like `Pair[E, E]` mean "both element types are the SAME" rather than
// "two independent holes": the second occurrence of E must unify with whatever
// the first occurrence bound it to. Non-linear patterns thus check equality for
// free, out of the same mechanism that does the binding.
static bool bind(ReflectBindings* b, const char* name, Type* concrete) {
    for (size_t i = 0; i < b->count; i++) {
        if (strcmp(b->names[i], name) == 0) {
            // already bound — must be consistent (same concrete type)
            return Type_Equals(b->args[i], concrete);
        }
    }
    if (b->count >= b->capacity) {
        b->capacity = b->capacity ? b->capacity * 2 : 4;
        b->names = (const char**)realloc(b->names, b->capacity * sizeof(char*));
        b->args  = (Type**)realloc(b->args,  b->capacity * sizeof(Type*));
    }
    b->names[b->count] = name;
    b->args[b->count]  = concrete;
    b->count++;
    return true;
}

// Wrap a concrete integer (an array's count) as a TYPE_CONST_VALUE pinned to u32,
// so an array-size hole `N` binds to the exact same value representation const
// generics use everywhere else. clone_ast's AST_IDENT case already knows how to
// lower a TYPE_CONST_VALUE-bound param into a pinned literal in the arm body;
// binding N this way means `E[N]` extracting `4` needs no new lowering at all.
static Type* const_value_u32(uint64_t v) {
    Type* pin = Type_MakePrim(PRIM_U32);
    Type* cv = (Type*)calloc(1, sizeof(Type));
    cv->cls = TYPE_CONST_VALUE;
    cv->cval.scalar = (int64_t)v;
    cv->cval.pin = pin;
    cv->cval.is_agg = false;
    cv->cval.defer = NULL;
    return cv;
}

// Recursively unify a concrete type against a pattern type, collecting wildcard
// bindings into `out`. Returns true iff the shapes match all the way down.
//
// The recursion mirrors Type_Equals exactly, with one extra rule at the top: if
// the pattern node is a hole, it matches anything and binds. Everywhere else the
// structural requirement is identical to equality — a pointer pattern demands a
// pointer concrete, an array pattern an array, etc. — so the two functions stay
// in obvious correspondence, and a shape the pattern can't destructure fails the
// same way a type inequality already does.
// Shared core of every pack-tail: given `count` concrete element types, a
// fixed prefix length, and the pack-tail's pattern hole, bundle every
// remaining concrete element (zero or more, past the fixed prefix, which
// each caller has already unified positionally itself -- struct fields and
// fn params don't share a common unify-one-element accessor, so that part
// stays in each case) into ONE freshly synthesized anon struct and bind it
// to the tail hole. `is_overlapping` lets a union's Rest tail stay a union
// rather than becoming a plain struct (see the struct case's own comment on
// this below) -- the function case always passes false, since fn params
// have no such concept.
//
// This is the one routine both TYPE_STRUCT's field-list pack tail and
// TYPE_FUNCTION's param-list pack tail drive, rather than each hand-rolling
// its own copy of "rebundle rest / bind" -- they differ only in how a
// concrete element type is fetched at index i, which the caller supplies as
// get_elem_type(ctx, i).
typedef Type* (*PackElemGetter)(void* ctx, size_t i);

static bool pack_tail_unify(size_t concrete_count, size_t fixed,
                            Type* tail_pat, PackElemGetter get_elem_type,
                            void* ctx, bool is_overlapping,
                            ReflectBindings* out) {
    if (concrete_count < fixed) return false;
    if (!is_hole(tail_pat)) return false; // pack-tail slot must be a bare wildcard
    size_t rest_n = concrete_count - fixed;
    Type** rest_types = (Type**)malloc((rest_n ? rest_n : 1) * sizeof(Type*));
    for (size_t i = 0; i < rest_n; i++) rest_types[i] = get_elem_type(ctx, fixed + i);
    StructDef* rest_sd = Struct_MakeAnon(rest_types, rest_n, is_overlapping);
    free(rest_types);
    Type* rest_type = (Type*)calloc(1, sizeof(Type));
    rest_type->cls = TYPE_STRUCT;
    rest_type->struct_name = rest_sd->name;
    return bind(out, tail_pat->param_name, rest_type);
}

static Type* fn_param_getter(void* ctx, size_t i) {
    return ((Type*)ctx)->function.param_types[i];
}

// Build the DECLARED view of a struct's field list for type-pattern matching.
// Struct_Layout stores a `super A base` field as its PROMOTED PREFIX (A's own
// fields, spliced inline) followed by the packaged alias field, which shares the
// prefix's storage. That dual representation is right for layout and for `d.tag`
// lookup, but it makes reflection show a struct that does not exist:
// `struct Derived { super Base info  u32 extra }` reflects as THREE fields
// (u32, Base, u32) whose sizes sum to more than sizeof(Derived), because the
// prefix and its package are both counted. A pattern should see the struct the
// way it was written -- `Base`, then `u32` -- so collapse each alias's promoted
// prefix back into the single `super` field it came from.
static size_t declared_fields(StructDef* sd, StructField** out) {
    size_t n = 0;
    for (size_t i = 0; i < sd->field_count; i++) {
        StructField* f = &sd->fields[i];
        if (f->is_super_alias) {
            uint32_t span = f->super_prefix_span;
            if ((size_t)span <= n) n -= span;   // drop the prefix already emitted
        }
        out[n++] = f;
    }
    return n;
}

// One field slot of a struct type-pattern against one concrete field. Shared by
// both anon-struct unify paths (pack-tail and strict-arity) so the `.name`
// assertion rule lives in exactly one place rather than being inlined twice --
// and so a later fn-param version can call the same routine.
static bool field_slot_unify(StructField* concrete, StructField* pat, ReflectBindings* out) {
    if (pat->name_asserted) {
        if (!concrete->name || !pat->name) return false;
        if (strcmp(concrete->name, pat->name) != 0) return false;
    }
    return reflect_unify(concrete->type, pat->type, out);
}

bool reflect_unify(Type* concrete, Type* pattern, ReflectBindings* out) {
    if (!concrete && !pattern) return true;

    // ── Tagged nominal pattern: `struct M`, `struct M[X]`, `enum M[X]`, ... ──
    // The tag states the KIND, which is the one fact brackets can never carry, so
    // it is checked against the concrete declaration. The head then binds to the
    // concrete's own template (left UNAPPLIED, so it stays usable as `M[u8]`
    // later), and each bracket argument unifies pairwise against the concrete
    // instantiation's type_args -- ordinary reflect_unify recursion, so nested
    // tagged applications, wildcards and concrete args all work at any depth.
    if (pattern && concrete && is_hole(pattern) && pattern->nominal_tag) {
        if (concrete->cls != TYPE_STRUCT || !concrete->struct_name) return false;
        StructDef* csd = Struct_Find(concrete->struct_name);
        if (!csd) return false;

        unsigned char actual = csd->is_enum ? 2 : csd->is_overlapping ? 3 : 1;
        if (actual != pattern->nominal_tag) return false;   // kind assertion

        StructDef* tmpl_sd = csd->generic_base ? csd->generic_base : csd;
        size_t nargs = csd->generic_base ? csd->type_arg_count : 0;
        if (pattern->app_arg_count != nargs) return false;  // arity from concrete

        Type* tmpl = (Type*)calloc(1, sizeof(Type));
        tmpl->cls = TYPE_STRUCT;
        tmpl->struct_name = tmpl_sd->name;
        tmpl->struct_unapplied = (nargs > 0);
        if (!bind(out, pattern->param_name, tmpl)) return false;

        for (size_t i = 0; i < nargs; i++)
            if (!reflect_unify(csd->type_args[i], pattern->app_args[i], out)) return false;
        return true;
    }

    // Pattern hole: bind it to whatever the concrete side is here, and succeed.
    if (pattern && is_hole(pattern)) {
        return bind(out, pattern->param_name, concrete);
    }

    // Same normalization Type_Equals' function-return-type case already applies
    // (see Type_IsVoidLike/fn_ret_equal in types.c): an OMITTED type (a no-payload
    // enum variant's stored NULL field type, reached here after a wildcard bound
    // to it -- e.g. `enum { H h  Rest... r }` then `match H { void {...} }`) must
    // compare equal to an EXPLICIT `void` pattern. Without this, H's binding
    // (concrete=NULL) never matched a literal `void` pattern, even though
    // nameof(H) already prints "void" for the identical value -- the STRING
    // representation and the STRUCTURAL comparison silently disagreed.
    if (Type_IsVoidLike(concrete) && Type_IsVoidLike(pattern)) return true;

    if (!concrete || !pattern) return false;

    // ── `impl { fn name(A) B }` ───────────────────────────────────────────────
    // The one pattern whose match is a SYMBOL LOOKUP rather than a structural walk.
    // Methods are not part of the type grammar: `impl P { fn free() }` lowers to a
    // free function `P_free(P* self)` in the global symbol table, and `p.free()` is
    // resolved by mangling + lookup (try_rewrite_method_call). So "does T have a
    // free()?" is answered by asking whether the symbol `T_free` exists -- exactly
    // the question the compiler already asks on every method call.
    //
    // The SIGNATURE check is not a name check: the found symbol's fn-type is unified
    // against the pattern's fn-type through this same reflect_unify, so wildcards in
    // the pattern (`fn map(A) B`) bind exactly as they do anywhere else. Nothing here
    // is a new matching semantics -- only a new source for the type being matched.
    if (pattern->cls == TYPE_IMPL) {
        // Only a struct can carry methods. Peel a pointer so `match T` works whether
        // the scrutinee is `P` or `P*`.
        Type* st = concrete;
        if (st->cls == TYPE_POINTER && st->pointer_base) st = st->pointer_base;
        if (st->cls != TYPE_STRUCT || !st->struct_name) return false;

        // Mangle exactly as try_rewrite_method_call does. For an INSTANTIATED generic
        // (Vector_i32) the methods live under the GENERIC BASE name (Vector_free), so
        // resolve through generic_base when present -- otherwise `match T` inside a
        // generic would never find its own methods.
        StructDef* sd = Struct_Find(st->struct_name);
        const char* base_name = (sd && sd->generic_base) ? sd->generic_base->name : st->struct_name;
        size_t blen = strlen(base_name);

        // EVERY method in the pattern must be present. The loop shares one `out`, so a
        // wildcard appearing in two signatures binds once and must AGREE the second
        // time -- bind()'s existing consistency check. That is what makes
        // `impl { fn get(u32) E  fn set(u32, E) void }` mean "a container whose get and
        // set agree on their element type" rather than two unrelated holes. An
        // associated type, for free, from machinery that already existed.
        for (size_t m = 0; m < pattern->impl_pat.method_count; m++) {
            size_t mlen = pattern->impl_pat.method_name_lens[m];
            size_t manglen = blen + 1 + mlen;
            char* mangled = (char*)malloc(manglen + 1);
            memcpy(mangled, base_name, blen);
            mangled[blen] = '_';
            memcpy(mangled + blen + 1, pattern->impl_pat.method_names[m], mlen);
            mangled[manglen] = '\0';
            Symbol* msym = SymTable_Find(Get_SymTable(), mangled, manglen);
            free(mangled);
            if (!msym || msym->kind != SYM_FUNCTION || !msym->type ||
                msym->type->cls != TYPE_FUNCTION)
                return false;

            // Strip the implicit `self`: the real symbol is fn(P*, ...args), but the
            // pattern spells only the args the user writes at a call site. Compare
            // arity against the pattern AFTER dropping self.
            Type*  found = msym->type;
            size_t found_argc = found->function.param_count;
            if (found_argc < 1) return false;           // a method always has self
            size_t real_argc = found_argc - 1;          // drop self
            Type*  pat_sig   = pattern->impl_pat.sigs[m];
            if (!pat_sig || real_argc != pat_sig->function.param_count) return false;

            // Unify each declared parameter, then the return type, through the ordinary
            // path -- so a wildcard in the pattern binds and stays consistent across the
            // whole signature, and a concrete type must match exactly.
            for (size_t i = 0; i < real_argc; i++) {
                if (!reflect_unify(found->function.param_types[i + 1],
                                   pat_sig->function.param_types[i], out))
                    return false;
            }
            // A void return is carried as NULL on one side and may be spelled `void` on
            // the other; treat "no return type in the pattern" as "don't care about the
            // return", and otherwise unify normally (NULL==NULL succeeds at the top).
            if (pat_sig->function.return_type &&
                !reflect_unify(found->function.return_type,
                               pat_sig->function.return_type, out))
                return false;
        }
        return true;
    }

    // VALUE hole: a const-generic value slot in the pattern (`N` in `Stack[E, N]`)
    // is carried as a TYPE_CONST_VALUE whose `defer` is an AST_IDENT naming the
    // wildcard (see parser.c parse_generic_value_arg). Bind that name to the
    // concrete value arg. The concrete side is itself a TYPE_CONST_VALUE (the
    // struct instantiation's folded arg), so we pass it through as the binding —
    // reflect_substitute_types then lowers `N` to its scalar in the arm body.
    if (pattern->cls == TYPE_CONST_VALUE && pattern->cval.defer &&
        pattern->cval.defer->type == AST_IDENT) {
        const char* wname = pattern->cval.defer->ident.name;
        if (concrete->cls == TYPE_CONST_VALUE)
            return bind(out, wname, concrete);
        return false; // value slot vs non-value concrete: shape mismatch
    }

    // A concrete param on the LEFT means the scrutinee itself is still abstract —
    // unification isn't meaningful yet (we're not at a concrete instantiation).
    // Fall back to nominal equality so a not-yet-instantiated `match` neither
    // wrongly matches nor crashes; the real unify happens per instantiation once
    // the scrutinee is concrete. (In practice the pipeline only reaches the
    // pattern fold after monomorphization has made the scrutinee concrete.)
    if (concrete->cls == TYPE_PARAM) {
        return Type_Equals(concrete, pattern);
    }

    // Shape must agree from here down, exactly as Type_Equals requires.
    if (concrete->cls != pattern->cls) return false;

    switch (pattern->cls) {
        case TYPE_PRIMITIVE:
            // A primitive in the pattern (`u32`, `f32`, ...) is a literal shape,
            // NOT a hole — it compares, it doesn't bind. This is the "shadowing"
            // rule from the spec: `match T { u32 { ... } }` compares T against the
            // real u32 type; it does not register a wildcard named u32.
            return concrete->primitive == pattern->primitive;

        case TYPE_POINTER:
            // `P*`: recurse into the pointee. P (or a deeper hole) binds there.
            return reflect_unify(concrete->pointer_base, pattern->pointer_base, out);

        case TYPE_ARRAY: {
            // `E[N]`: element unifies structurally; size either compares (literal
            // size in the pattern) or binds (size hole).
            if (!reflect_unify(concrete->array.element, pattern->array.element, out))
                return false;

            // A size hole is a TYPE_PARAM sitting in the count position. The parser
            // records that by leaving array.count == 0 and stashing the param name
            // in array.size_param (see parser.c in_match_pattern). Bind it to the
            // concrete count as a pinned value. Otherwise it's a literal size and
            // must equal the concrete count.
            if (pattern->array.size_param) {
                return bind(out, pattern->array.size_param,
                            const_value_u32(concrete->array.count));
            }
            return concrete->array.count == pattern->array.count;
        }

        case TYPE_FUNCTION: {
            // Prototype: `fn(Fixed, Rest...) R` pack-tail, driven by the shared
            // pack_tail_unify core above -- same routine the struct-field pack
            // tail below drives, just fed function params via fn_param_getter
            // instead of struct fields. Unify the fixed prefix positionally here
            // first (pack_tail_unify only handles the rebundle+bind of the tail),
            // then hand off.
            if (pattern->function.pack_param_index >= 0) {
                size_t fixed = (size_t)pattern->function.pack_param_index;
                if (concrete->function.param_count < fixed) return false;
                for (size_t i = 0; i < fixed; i++) {
                    if (!reflect_unify(concrete->function.param_types[i],
                                       pattern->function.param_types[i], out))
                        return false;
                }
                if (!pack_tail_unify(concrete->function.param_count, fixed,
                                     pattern->function.param_types[fixed],
                                     fn_param_getter, (void*)concrete, false, out))
                    return false;
                return reflect_unify(concrete->function.return_type,
                                     pattern->function.return_type, out);
            }

            // `fn(A) B`: arg and return types unify positionally; arity must match.
            if (concrete->function.param_count != pattern->function.param_count)
                return false;
            for (size_t i = 0; i < pattern->function.param_count; i++) {
                if (!reflect_unify(concrete->function.param_types[i],
                                   pattern->function.param_types[i], out))
                    return false;
            }
            return reflect_unify(concrete->function.return_type,
                                 pattern->function.return_type, out);
        }

        case TYPE_STRUCT: {
            // Two struct types. If both are instantiations of the SAME generic base
            // (e.g. concrete `Box[u64]` vs pattern `Box[E]`), destructure: unify
            // their type-args positionally, so a wildcard in the pattern's args binds
            // to the concrete arg (`E` -> u64). This is the struct analogue of the
            // pointer/array/function cases — it reads the shape out of the type
            // system's own generic-instantiation record, no field poking, staying
            // within the spec's "no arbitrary struct-field destructuring" rule.
            StructDef* cs = Struct_Find(concrete->struct_name);
            StructDef* ps = Struct_Find(pattern->struct_name);

            // Anonymous struct PATTERN: `struct { A x  B y }` destructures the
            // concrete struct's FIELDS positionally — the field-list analogue of the
            // generic-args case below and of `fn(A) B`'s param list. Field names are
            // ignored (like fn param names); only field TYPES and arity unify. This
            // is the one place a nominal struct is looked *inside* — it does not make
            // structs structurally typed, it only lets a pattern read the fields the
            // StructDef already holds. Reuses the same recursion, so a field that is
            // itself a pointer/array/generic destructures further for free.
            // The anonymous pattern's KIND must match the concrete's. `struct`, `union`
            // and `enum` all register as anonymous StructDefs (they share the registry,
            // differing only in is_enum/is_overlapping), so without this check a
            // `struct { A x  B y }` pattern would destructure a concrete UNION -- same
            // field list, completely different layout and meaning. The declaration path
            // never had to check this because a named type carries its kind in its
            // identity; an anonymous one carries it in these flags.
            if (ps && ps->is_anonymous && cs &&
                cs->is_enum == ps->is_enum &&
                cs->is_overlapping == ps->is_overlapping) {
                // Prototype: a pack-tail field (`Rest... rest`, ps->pack_field_index
                // >= 0) relaxes the strict-arity rule below. Fields before the pack
                // tail unify positionally exactly as always; every remaining concrete
                // field (zero or more) gets rebundled into ONE freshly synthesized
                // anon struct via Struct_MakeAnon -- the same call used to build a
                // `T... args` pack bundle at a call site (types.c) -- and that struct
                // is unified against the pack-tail's own pattern type, which is just
                // an ordinary hole and binds through the existing path at the top of
                // this function. Zero leftover fields still works: Struct_MakeAnon
                // with field_count==0 produces the same empty anon struct `struct {}`
                // already matches today, so the base case of a peeling recursion
                // needs no special-casing here.
                if (ps->pack_field_index >= 0) {
                    size_t fixed = (size_t)ps->pack_field_index;
                    StructField** dfs = (StructField**)malloc((cs->field_count ? cs->field_count : 1) * sizeof(StructField*));
                    size_t dcount = declared_fields(cs, dfs);
                    if (dcount < fixed) { free(dfs); return false; }
                    for (size_t i = 0; i < fixed; i++) {
                        if (!field_slot_unify(dfs[i], &ps->fields[i], out))
                            { free(dfs); return false; }
                    }
                    size_t rest_n = dcount - fixed;
                    Type** rest_types = (Type**)malloc((rest_n ? rest_n : 1) * sizeof(Type*));
                    for (size_t i = 0; i < rest_n; i++) rest_types[i] = dfs[fixed + i]->type;
                    StructDef* rest_sd;
                    if (cs->is_enum) {
                        // The tail of an ENUM's variant list is a smaller enum, not a
                        // struct -- and it must keep each remaining variant's ORIGINAL
                        // absolute tag (Enum_VariantIndex/match's tag-compare codegen
                        // read variant_tag, not array position), so the rebundle stays
                        // a valid reinterpretation of the SAME bytes as the concrete
                        // type, exactly like a struct-tail already is.
                        uint32_t* rest_tags = (uint32_t*)malloc((rest_n ? rest_n : 1) * sizeof(uint32_t));
                        for (size_t i = 0; i < rest_n; i++) rest_tags[i] = dfs[fixed + i]->variant_tag;
                        rest_sd = Struct_MakeAnonEnum(rest_types, rest_tags, rest_n);
                        free(rest_tags);
                    } else {
                        // Preserve is_overlapping too: a union's Rest tail must stay a
                        // union (fields overlapping at offset 0), not silently become a
                        // struct with fields stacked one after another -- same class of
                        // bug as the enum tag issue above, just on the layout axis
                        // instead of the tag axis. A plain struct's Rest is unaffected
                        // (is_overlapping is already false there).
                        rest_sd = Struct_MakeAnon(rest_types, rest_n, cs->is_overlapping);
                    }
                    free(rest_types); free(dfs);
                    Type* rest_type = (Type*)calloc(1, sizeof(Type));
                    rest_type->cls = TYPE_STRUCT;
                    rest_type->struct_name = rest_sd->name;
                    return reflect_unify(rest_type, ps->fields[fixed].type, out);
                }
                {
                    StructField** dfs = (StructField**)malloc((cs->field_count ? cs->field_count : 1) * sizeof(StructField*));
                    size_t dcount = declared_fields(cs, dfs);
                    if (dcount != ps->field_count) { free(dfs); return false; }
                    for (size_t i = 0; i < ps->field_count; i++) {
                        if (!field_slot_unify(dfs[i], &ps->fields[i], out)) {
                            free(dfs); return false;
                        }
                    }
                    free(dfs);
                    return true;
                }
            }

            if (cs && ps && cs->generic_base && ps->generic_base &&
                cs->generic_base == ps->generic_base &&
                cs->type_arg_count == ps->type_arg_count) {
                for (size_t i = 0; i < ps->type_arg_count; i++) {
                    if (!reflect_unify(cs->type_args[i], ps->type_args[i], out))
                        return false;
                }
                return true;
            }
            // Otherwise nominal: same named type only. A bare hole `S` still matches
            // any type via is_hole() at the top; this fires for a literal named type.
            return strcmp(concrete->struct_name, pattern->struct_name) == 0;
        }

        default:
            // Any other class (const-value, etc.): fall back to plain equality.
            return Type_Equals(concrete, pattern);
    }
}

void reflect_bindings_free(ReflectBindings* b) {
    free(b->names);
    free(b->args);
    b->names = NULL; b->args = NULL; b->count = b->capacity = 0;
}

