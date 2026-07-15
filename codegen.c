// codegen.c — backend-neutral layout traversal shared by every value sink.
//
// "How to lay out an aggregate literal" (walk a struct/array literal, place each
// field/element at its offset, recurse into nested aggregates, write the enum
// tag) is ONE algorithm. It used to live welded to x86 inside emit_fill_literal.
// Here it is expressed once, in terms the type system already provides
// (StructField.offset, Type_SizeOf, Type_Width, Type_IsFloat), and driven
// through a LayoutSink whose leaf actions differ per consumer:
//   * x86 backend  -> emit machine-code stores
//   * constexpr    -> write bytes into the comptime arena
//
// The traversal owns LAYOUT; the sink owns EVALUATION + PLACEMENT. put_scalar
// receives the AST node (not a value) so the backend can emit code to compute it
// while constexpr folds it — same traversal, two evaluation strategies.
//
// See docs/codegen_sink_design.md.

#include "compiler.h"
#include "codegen.h"
#include <string.h>

// Walk an aggregate literal, driving `sink` at each leaf. Offsets are relative to
// the current aggregate base (the sink tracks the base via enter_sub/leave_sub).
// Returns false if any leaf is unplaceable (the sink decides — e.g. constexpr
// rejects a non-closed-term value).
bool layout_fill(ASTNode* lit, const LayoutSink* sink) {
    if (!lit) return false;

    if (lit->type == AST_STRUCT_LITERAL) {
        StructDef* sd = lit->struct_lit.sdef;
        if (!sd) return false; // unresolved literal: caller diagnoses elsewhere

        // Enum: variant tag (u32) at offset 0; the payload (if any) is a normal
        // field handled by the loop below (variant is a real StructField).
        if (sd->is_enum) {
            int tag = Enum_VariantIndex(sd, lit->struct_lit.field_names[0],
                                        lit->struct_lit.field_name_lens[0]);
            if (!sink->put_tag(sink->ctx, tag)) return false;
        }

        // Field defaults first (storage is pre-zeroed by the sink); explicit
        // fields below override.
        for (size_t i = 0; i < sd->field_count; i++) {
            StructField* f = &sd->fields[i];
            if (!f->has_default) continue;
            if (!sink->put_default(sink->ctx, f->offset, Type_SizeOf(f->type),
                                   f->default_val_buf)) return false;
        }

        // Explicit fields.
        for (size_t i = 0; i < lit->struct_lit.count; i++) {
            StructField* f = Struct_FindField(sd, lit->struct_lit.field_names[i],
                                              lit->struct_lit.field_name_lens[i]);
            if (!f) return false;
            ASTNode* val = lit->struct_lit.values[i];
            bool agg = Type_IsAggregate(f->type);
            if (agg) {
                if (val->type == AST_STRUCT_LITERAL || val->type == AST_ARRAY_LITERAL) {
                    if (!sink->enter_sub(sink->ctx, f->offset)) return false;
                    bool ok = layout_fill(val, sink);
                    sink->leave_sub(sink->ctx);
                    if (!ok) return false;
                } else {
                    if (!sink->put_agg_value(sink->ctx, f->offset,
                                             Type_SizeOf(f->type), val)) return false;
                }
            } else {
                if (!sink->put_scalar(sink->ctx, f->offset, Type_Width(f->type),
                                      Type_IsFloat(f->type), val)) return false;
            }
        }
        return true;
    }

    if (lit->type == AST_ARRAY_LITERAL) {
        Type* et = lit->array_lit.elem_type;
        if (!et) return false;
        uint64_t esz = Type_SizeOf(et);
        bool agg = Type_IsAggregate(et);
        for (size_t i = 0; i < lit->array_lit.count; i++) {
            ASTNode* val = lit->array_lit.values[i];
            uint64_t off = i * esz;
            if (agg) {
                if (val->type == AST_STRUCT_LITERAL || val->type == AST_ARRAY_LITERAL) {
                    if (!sink->enter_sub(sink->ctx, off)) return false;
                    bool ok = layout_fill(val, sink);
                    sink->leave_sub(sink->ctx);
                    if (!ok) return false;
                } else {
                    if (!sink->put_agg_value(sink->ctx, off, esz, val)) return false;
                }
            } else {
                if (!sink->put_scalar(sink->ctx, off, Type_Width(et),
                                      Type_IsFloat(et), val)) return false;
            }
        }
        return true;
    }

    return false;
}

// ─── Shared aggregate-binop traversal ──────────────────────────────────────

// Flatten an aggregate into scalar integer lanes (offset,width) for lanewise
// binops. Recurses through nested structs/arrays; padding simply has no lane.
// Returns lane count, or -1 on float/pointer/enum lanes (unsupported).
// (Moved here from backend_x64.c's static Agg_Lanes -- unchanged logic, just
// no longer welded to one consumer. Was already pure: only touches Type*.)
int Agg_Lanes(Type* t, uint64_t base, uint64_t* offs, uint64_t* ws, int n, int cap) {
    if (t->cls == TYPE_ARRAY) {
        uint64_t esz = Type_SizeOf(t->array.element);
        for (uint64_t i = 0; i < t->array.count; i++)
            if ((n = Agg_Lanes(t->array.element, base + i*esz, offs, ws, n, cap)) < 0) return -1;
        return n;
    }
    if (t->cls == TYPE_STRUCT) {
        StructDef* sd = Struct_Find(t->struct_name);
        if (!sd) return -1;
        if (sd->is_enum) {
            // Enums only support lanewise ==/!=, never arithmetic (no sane meaning
            // for tag/payload add-xor-etc). agg_binop_apply already restricts the
            // callable op set before it ever calls us for a non-eq op on a struct,
            // but an enum's *type* doesn't know which op is being attempted here,
            // so eq-only-ness is enforced by the caller passing is_eq through --
            // see agg_binop_apply, which now allows enums to reach Agg_Lanes only
            // when op is ==/!=. Tag (u32 @ offset 0) plus the full payload region
            // are walked as ordinary lanes: unused payload bytes are always
            // zero-filled at construction (see layout_fill's put_tag/pre-zeroed
            // storage), so a flat lanewise compare already gives correct tagged-
            // union equality (mismatched tags differ in the tag lane alone;
            // matched tags with no/zeroed payload compare equal) with no separate
            // tag-then-payload short-circuit needed.
            offs[n] = base; ws[n] = 4; n++; // u32 tag
            if (n >= cap) return -1;
            for (size_t f = 0; f < sd->field_count; f++) {
                if (!sd->fields[f].type) continue; // payloadless variant: no lane to add
                if ((n = Agg_Lanes(sd->fields[f].type, base + sd->fields[f].offset, offs, ws, n, cap)) < 0) return -1;
            }
            return n;
        }
        for (size_t f = 0; f < sd->field_count; f++)
            if ((n = Agg_Lanes(sd->fields[f].type, base + sd->fields[f].offset, offs, ws, n, cap)) < 0) return -1;
        return n;
    }
    if (t->cls != TYPE_PRIMITIVE || Type_IsFloat(t) || n >= cap) return -1;
    offs[n] = base; ws[n] = Type_Width(t);
    return n + 1;
}

// The single source of truth for "which binops are legal on struct/array
// operands": ==, !=, +, -, *, &, |, ^ (relational, /, %, shifts are not --
// no meaningful order or division on an aggregate). Previously this set was
// encoded once in backend_x64.c's switch and NOT enforced anywhere else --
// constexpr had no aggregate-binop path at all except a hand-written EQ/NEQ
// special case that didn't consult this list. Both consumers now call this
// function, so the legal-op set can't drift between JIT and constexpr again.
bool agg_binop_apply(ASTNodeType op, Type* lt, Type* rt, const AggBinopSink* sink) {
    if (!lt || !rt || !Type_Equals(lt, rt)) return false;

    bool is_eq = (op == AST_EQ || op == AST_NEQ);
    if (!is_eq) {
        switch (op) {
            case AST_ADD: case AST_SUB: case AST_MUL:
            case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
                break;
            default:
                return false; // not in the aggregate-legal set
        }
    }

    // Enums are only ever legal lanewise for ==/!=; reject arithmetic/bitwise
    // ops up front so Agg_Lanes never has to know which op is in flight.
    if (!is_eq) {
        StructDef* lsd = (lt->cls == TYPE_STRUCT) ? Struct_Find(lt->struct_name) : NULL;
        if (lsd && lsd->is_enum) return false;
    }

    uint64_t offs[256], ws[256];
    int nl = Agg_Lanes(lt, 0, offs, ws, 0, 256);
    if (nl < 0) return false; // float/pointer lane, or too many lanes

    for (int i = 0; i < nl; i++)
        if (!sink->do_lane(sink->ctx, op, offs[i], (int)ws[i], is_eq)) return false;

    if (is_eq) {
        int64_t dummy;
        if (!sink->finish_eq(sink->ctx, op == AST_EQ, &dummy)) return false;
    }
    return true;
}
