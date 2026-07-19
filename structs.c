#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StructDef** s_structs = NULL;
size_t s_struct_count = 0;
size_t s_struct_cap = 0;

StructDef* Struct_Find(const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < s_struct_count; i++) {
        if (strcmp(s_structs[i]->name, name) == 0) return s_structs[i];
    }
    return NULL;
}

StructDef* Struct_Register(const char* name, size_t len) {
    char* key = strndup(name, len);
    StructDef* existing = Struct_Find(key);
    if (existing) { free(key); return existing; }
    if (s_struct_count >= s_struct_cap) {
        s_struct_cap = s_struct_cap ? s_struct_cap * 2 : 64;
        s_structs = realloc(s_structs, s_struct_cap * sizeof(StructDef*));
    }
    StructDef* sd = calloc(1, sizeof(StructDef));
    sd->name = key;
    sd->laid_out = false;
    s_structs[s_struct_count++] = sd;
    return sd;
}

StructDef* Struct_MakeAnon(Type** field_types, size_t field_count, bool is_overlapping) {
    // Content-derived name, same scheme as the parser's `struct { A x  B y }`
    // literal path: "struct{<t0>,<t1>,...}" -- structural identity == name
    // identity, so a pack shape reused across call sites dedups for free.
    // A union-tail rebundle gets its own "union{...}" name (is_overlapping
    // folded into the key) so it can never collide/dedupe against the
    // struct-shaped rebundle of the same field types -- same layout-affecting
    // flag, same reasoning as is_enum getting its own name prefix below.
    char namebuf[1024];
    size_t off = 0;
    off += snprintf(namebuf + off, sizeof(namebuf) - off, is_overlapping ? "union{" : "struct{");
    for (size_t i = 0; i < field_count; i++) {
        char tn[128];
        Type_ToString(field_types[i], tn, sizeof(tn));
        off += snprintf(namebuf + off, sizeof(namebuf) - off, "%s%s",
                         tn, (i + 1 < field_count) ? "," : "");
    }
    snprintf(namebuf + off, sizeof(namebuf) - off, "}");

    StructDef* sd = Struct_Register(namebuf, strlen(namebuf));
    if (sd->field_count == 0 && !sd->laid_out) {
        sd->is_enum = false;
        sd->is_overlapping = is_overlapping;
        sd->is_anonymous = true;
        // Struct_MakeAnon always builds from already-concrete field types (a call-site
        // pack bundle or a reflect_unify pack-tail rebundle) -- never a pattern itself,
        // so it never has a pack-tail field of its own. Explicit, not relying on
        // calloc's zero-init (see the pack_field_index comment in compiler.h).
        sd->pack_field_index = -1;
        sd->fields = (StructField*)calloc(field_count ? field_count : 1, sizeof(StructField));
        sd->field_count = field_count;
        for (size_t i = 0; i < field_count; i++) {
            char fnbuf[16];
            snprintf(fnbuf, sizeof(fnbuf), "_%zu", i);
            sd->fields[i].name = strdup(fnbuf);
            sd->fields[i].type = field_types[i];
            sd->fields[i].offset = 0;
        }
        Struct_Layout(sd);
    }
    return sd;
}

// Same rebundling idea as Struct_MakeAnon, but for the TAIL of an enum's
// variant list (a `Rest...` pack-tail peel on an enum type -- see reflect_unify's
// pack-tail branch in reflections.c). An enum-tail is a real, smaller enum, not a
// struct: it needs is_enum=true and, critically, each variant's ORIGINAL absolute
// tag preserved (variant_tags[i]) rather than renumbered to 0,1,2... -- the whole
// point is that concrete VALUES of the original enum are still valid values of
// this rebundled type (same bytes, same tag meaning), so match/Enum_VariantIndex
// on the rebundle must agree with match on the original. Variant names are
// synthesized (like Struct_MakeAnon's field names) since nothing generic reads
// them back off the REBUNDLE -- the established idiom (see describe.t,
// generic_json.t) is nameof(Orig, N) against the ORIGINAL type + a tracked index,
// never nameof(Rest, i) against a peeled tail.
StructDef* Struct_MakeAnonEnum(Type** variant_types, uint32_t* variant_tags, size_t variant_count) {
    char namebuf[1024];
    size_t off = 0;
    off += snprintf(namebuf + off, sizeof(namebuf) - off, "enum{");
    for (size_t i = 0; i < variant_count; i++) {
        char tn[128];
        if (variant_types[i]) Type_ToString(variant_types[i], tn, sizeof(tn));
        else snprintf(tn, sizeof(tn), "void");
        off += snprintf(namebuf + off, sizeof(namebuf) - off, "%u:%s%s",
                         variant_tags[i], tn, (i + 1 < variant_count) ? "," : "");
    }
    snprintf(namebuf + off, sizeof(namebuf) - off, "}");

    StructDef* sd = Struct_Register(namebuf, strlen(namebuf));
    if (sd->field_count == 0 && !sd->laid_out) {
        sd->is_enum = true;
        sd->is_anonymous = true;
        sd->pack_field_index = -1;
        sd->fields = (StructField*)calloc(variant_count ? variant_count : 1, sizeof(StructField));
        sd->field_count = variant_count;
        for (size_t i = 0; i < variant_count; i++) {
            char fnbuf[16];
            snprintf(fnbuf, sizeof(fnbuf), "_%u", variant_tags[i]);
            sd->fields[i].name = strdup(fnbuf);
            sd->fields[i].type = variant_types[i];
            sd->fields[i].offset = 0;
            sd->fields[i].variant_tag = variant_tags[i];
        }
        Struct_Layout(sd);
    }
    return sd;
}

StructDef** Struct_GetAll(size_t* out_count) {
    *out_count = s_struct_count;
    return s_structs;
}

// See declaration in compiler.h. `proto.name` is duplicated here so callers
// never have to remember to strdup a borrowed name themselves.
void Struct_AppendField(StructField** fields, size_t* count, size_t* cap, StructField proto) {
    if (*count >= *cap) {
        *cap = (*cap) ? (*cap) * 2 : 4;
        *fields = realloc(*fields, (*cap) * sizeof(StructField));
    }
    StructField* f = &(*fields)[(*count)++];
    f->name = strdup(proto.name);
    f->type = proto.type;
    f->offset = 0;
    f->has_default = proto.has_default;
    f->default_val_buf = proto.default_val_buf;
    f->is_super_param = false;
    f->variant_tag = proto.variant_tag; // preserve on copy (super splice, generic instantiation)
    f->is_super_alias = proto.is_super_alias;       // preserve the prefix-alias flag on copy
    f->super_prefix_span = proto.super_prefix_span;
}

StructDef* Struct_Instantiate(StructDef* gen, Type** targs, size_t targ_count) {
    if (!gen || !gen->is_generic) return NULL;
    if (targ_count != gen->type_param_count) {
        fprintf(stderr, "Error: generic %s '%s' expects %zu type arguments, got %zu\n",
                gen->is_overlapping ? "union" : "struct", gen->name, gen->type_param_count, targ_count);
        exit(1);
    }
    
    for (size_t i = 0; i < s_struct_count; i++) {
        StructDef* inst = s_structs[i];
        if (inst->generic_base == gen && inst->type_arg_count == targ_count) {
            bool match = true;
            for (size_t j = 0; j < targ_count; j++) {
                if (!Type_Equals(inst->type_args[j], targs[j])) {
                    match = false; break;
                }
            }
            if (match) return inst;
        }
    }
    
    StructDef* inst = calloc(1, sizeof(StructDef));
    char namebuf[512];
    snprintf(namebuf, sizeof(namebuf), "%s[", gen->name);
    for (size_t j = 0; j < targ_count; j++) {
        char tname[128];
        Type_ToString(targs[j], tname, sizeof(tname));
        strncat(namebuf, tname, sizeof(namebuf) - strlen(namebuf) - 1);
        if (j < targ_count - 1) strncat(namebuf, ", ", sizeof(namebuf) - strlen(namebuf) - 1);
    }
    strncat(namebuf, "]", sizeof(namebuf) - strlen(namebuf) - 1);
    inst->name = strdup(namebuf);
    inst->is_enum = gen->is_enum;
    inst->is_overlapping = gen->is_overlapping;
    inst->is_generic = false;
    inst->generic_base = gen;
    inst->type_args = malloc(targ_count * sizeof(Type*));
    for (size_t i = 0; i < targ_count; i++) inst->type_args[i] = targs[i];
    inst->type_arg_count = targ_count;
    inst->laid_out = false;
    inst->laying_out = false;
    
    if (s_struct_count >= s_struct_cap) {
        s_struct_cap = s_struct_cap ? s_struct_cap * 2 : 64;
        s_structs = realloc(s_structs, s_struct_cap * sizeof(StructDef*));
    }
    s_structs[s_struct_count++] = inst;
    
    if (gen->field_count > 0) {
        // Ordinarily this is a 1:1 copy (substitute each field's type, same count,
        // same order) -- but a `super T base` field (is_super_param) expands into
        // MULTIPLE instance fields once T is bound to a concrete struct: the
        // promoted fields of that struct, then `base` itself. So this can't be a
        // fixed-size calloc keyed on gen->field_count; grow as we go, same pattern
        // parse_struct_decl_ex already uses for the non-generic `super` splice.
        size_t icap = 0;
        inst->fields = NULL;
        inst->field_count = 0;
        for (size_t i = 0; i < gen->field_count; i++) {
            StructField* gf = &gen->fields[i];
            Type* subst_type = gf->type ? Type_Substitute(gf->type, gen->type_params, inst->type_args, inst->type_arg_count) : NULL;

            if (gf->is_super_param && subst_type && subst_type->cls == TYPE_STRUCT) {
                // T resolved to a concrete struct: splice ITS promoted fields in at
                // this position (same expansion the non-generic `super A base` path
                // performs at parse time), then the base field itself.
                StructDef* super_sd = Struct_Find(subst_type->struct_name);
                uint32_t span = 0;
                if (super_sd) {
                    for (size_t si = 0; si < super_sd->field_count; si++) {
                        Struct_AppendField(&inst->fields, &inst->field_count, &icap, super_sd->fields[si]);
                    }
                    span = (uint32_t)super_sd->field_count;
                }
                // Packaged field aliases the just-spliced prefix (single storage),
                // matching the non-generic `super A base` path.
                StructField base_f = { .name = gf->name, .type = subst_type,
                    .has_default = gf->has_default, .default_val_buf = gf->default_val_buf,
                    .is_super_alias = true, .super_prefix_span = span };
                Struct_AppendField(&inst->fields, &inst->field_count, &icap, base_f);
                continue;
            }

            StructField f = { .name = gf->name, .type = subst_type,
                .has_default = gf->has_default, .default_val_buf = gf->default_val_buf,
                .variant_tag = gf->variant_tag };
            Struct_AppendField(&inst->fields, &inst->field_count, &icap, f);
        }
    }
    Struct_Layout(inst);
    return inst;
}

// Called after a generic template is fully parsed. Any instantiations (like self-referential
// pointers Node[T] -> Node$TT) created *during* the parse will have 0 fields. This backfills them.
void Struct_UpdateInstantiations(StructDef* gen) {
    if (!gen || !gen->is_generic) return;
    for (size_t i = 0; i < s_struct_count; i++) {
        StructDef* inst = s_structs[i];
        if (inst->generic_base == gen && inst->field_count != gen->field_count && gen->field_count > 0) {
            inst->field_count = gen->field_count;
            inst->fields = (StructField*)calloc(gen->field_count, sizeof(StructField));
            for (size_t j = 0; j < gen->field_count; j++) {
                inst->fields[j].name = strdup(gen->fields[j].name);
                inst->fields[j].type = gen->fields[j].type
                    ? Type_Substitute(gen->fields[j].type, gen->type_params, inst->type_args, inst->type_arg_count)
                    : NULL;
                inst->fields[j].has_default = gen->fields[j].has_default;
                inst->fields[j].default_val_buf = gen->fields[j].default_val_buf;
                inst->fields[j].variant_tag = gen->fields[j].variant_tag;
            }
            Struct_Layout(inst);
        }
    }
}

StructField* Struct_FindField(StructDef* sd, const char* name, size_t len) {
    if (!sd) return NULL;
    for (size_t i = 0; i < sd->field_count; i++) {
        if (strlen(sd->fields[i].name) == len &&
            strncmp(sd->fields[i].name, name, len) == 0) {
            return &sd->fields[i];
        }
    }
    return NULL;
}

// The TAG VALUE of variant `name` in enum `sd` -- NOT necessarily its index in
// fields[]. Ordinarily the two coincide (a plain `enum Foo { A B C }` numbers
// tags 0,1,2 in declaration order, same as array position) -- they diverge
// only for an anonymous sub-enum built by peeling a `Rest...` pack-tail off a
// concrete enum (Struct_MakeAnon), whose fields[0] keeps its ORIGINAL absolute
// tag instead of renumbering to 0. Returns -1 if not found.
int Enum_VariantIndex(StructDef* sd, const char* name, size_t len) {
    StructField* f = Struct_FindField(sd, name, len);
    return f ? (int)f->variant_tag : -1;
}

uint64_t Type_SizeOf(const Type* t) {
    if (!t) return 8;
    switch (t->cls) {
        case TYPE_PRIMITIVE: {
            // Shares Prim_Width's table (types.c) for every real primitive, instead of
            // hand-retyping the same u8..f64 switch a second time -- which used to be
            // exactly what this did, and had silently drifted from Type_Width's copy
            // for the void case (this one returned 0, Type_Width's returned 8). void's
            // 0-byte STORAGE size is still decided here, deliberately, not by accident.
            int w = Prim_Width(t->primitive);
            return w >= 0 ? (uint64_t)w : 0;
        }
        case TYPE_POINTER: return 8;
        case TYPE_FUNCTION: return 8; // function pointer
        case TYPE_FN_LITERAL: return 8; // same runtime rep as TYPE_FUNCTION
        case TYPE_PARAM: return 8; // params are pointers/words inside generic templates
        case TYPE_ARRAY: return t->array.count * Type_SizeOf(t->array.element);
        case TYPE_STRUCT: {
            StructDef* sd = Struct_Find(t->struct_name);
            if (!sd) return 0;
            Struct_Layout(sd); // ensure size is computed
            return sd->size;
        }
    }
    return 8;
}

// Lay out a struct: assign back-to-back offsets, compute total size.
// Recursive containment by value => compile error (cycle detection here).
void Struct_Layout(StructDef* sd) {
    if (!sd || sd->laid_out) return;
    if (sd->is_generic) return; // a generic template has no single layout
    if (sd->laying_out) {
        fprintf(stderr, "Error: %s '%s' contains itself by value (recursive layout)\n", sd->is_overlapping ? "union" : "struct", sd->name);
        exit(1);
    }
    sd->laying_out = true;

    // An enum reserves a tag at offset 0, then OVERLAPS all variant payloads in one
    // region after it: every variant's payload sits at the same offset (TAG_SIZE), and
    // the type's size is TAG_SIZE + the largest payload. A union is the same overlap
    // with no tag (TAG_SIZE 0). A plain struct instead lays fields back-to-back
    // (offset accumulates, size = sum). Same loop, same cycle detection, same
    // Type_SizeOf -- only the offset/size arithmetic differs.
    bool overlapping = sd->is_enum || sd->is_overlapping;
    const uint64_t TAG_SIZE = sd->is_enum ? 4 : 0; // u32 discriminant; none for a union
    uint64_t max_align = 1;
    uint64_t max_payload_extent = 0;
    uint64_t off = TAG_SIZE;

    if (sd->is_enum) {
        max_align = 4; // the tag is u32, so enum alignment is at least 4
    }

    for (size_t i = 0; i < sd->field_count; i++) {
        StructField* f = &sd->fields[i];
        if (f->type && f->type->cls == TYPE_STRUCT) {
            StructDef* inner = Struct_Find(f->type->struct_name);
            Struct_Layout(inner);
        }

        // A `super A base` packaged field aliases the promoted prefix instead of
        // owning storage: point it at the first promoted field's offset and skip
        // the cursor advance (zero size contribution). super_prefix_span promoted
        // fields immediately precede it, so the prefix starts at fields[i-span].
        // (Aligned to falign too, so d.base and the prefix share the same address.)
        if (f->is_super_alias) {
            uint32_t span = f->super_prefix_span;
            f->offset = (span && i >= span) ? sd->fields[i - span].offset : f->offset;
            continue;
        }

        uint64_t fsize = f->type ? Type_SizeOf(f->type) : 0;
        uint64_t falign = f->type ? Type_AlignOf(f->type) : 1;
        if (falign > max_align) max_align = falign;

        if (overlapping) {
            // Variant/field payload starts at TAG_SIZE but must be aligned to falign
            uint64_t p_off = (TAG_SIZE + falign - 1) & ~(falign - 1);
            f->offset = p_off;
            if (p_off + fsize > max_payload_extent) max_payload_extent = p_off + fsize;
        } else {
            off = (off + falign - 1) & ~(falign - 1);
            f->offset = off;
            off += fsize;
        }
    }

    if (overlapping) {
        sd->size = (max_payload_extent + max_align - 1) & ~(max_align - 1);
        sd->align = max_align;
    } else {
        sd->size = (off + max_align - 1) & ~(max_align - 1);
        sd->align = max_align;
    }

    sd->laying_out = false;
    sd->laid_out = true;
}
