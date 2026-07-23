#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

// External declarations from parser/types/symtable needed for match lowering
extern void Lexer_Save(LexerState* state);
extern void Lexer_Restore(const LexerState* state);
extern Token Lexer_NextToken(void);
extern void advance(void);
extern void parse_error(const char* message);
extern SymbolTable* SymTable_Create(SymbolTable* parent);
extern Symbol* SymTable_Add(SymbolTable* table, const char* name, size_t length, Type* type, SymbolKind kind);
extern ASTNode* new_node(ASTNodeType type);
extern Type* parse_type(void);
extern ASTNode* parse_expr_prec(int min_prec);
extern ASTNode* parse_block_body(void);
extern ASTNode* parse_enum_variant_after_dot(void);
extern bool base_is_lvalue(ASTNode* node);
extern Type* Type_MakePrim(int primitive_kind);
extern void append_block_statement(ASTNode* block, ASTNode* stmt);
extern ASTNode* make_decl_stmt(Type* var_type, const char* name, size_t name_len, Symbol* sym, ASTNode* init_expr);
extern Type* make_pointer_type(Type* base);
extern void resolve_brace_literal(ASTNode* node, Type* target);
extern bool ConstEval_Bytes(ASTNode* node, uint8_t* out_buf, uint64_t size);
extern bool ConstEval(ASTNode* node, int64_t* out);
extern int Enum_VariantIndex(StructDef* sd, const char* name, size_t len);

extern SymbolTable* s_symtable;
extern Token s_curr;
extern size_t s_match_wildcard_count;
extern const char** s_match_wildcards;
extern bool* s_match_wc_is_size;
extern bool s_in_match_pattern;
extern bool s_pattern_types_ok;
extern const char** s_type_params;
extern Type** s_param_kinds;
extern size_t s_type_param_count;

#define DA_PUSH(arr, count, cap, val) \
    do { \
        if ((count) >= (cap)) { \
            (cap) = (cap) ? (cap) * 2 : 8; \
            (arr) = realloc((arr), (cap) * sizeof(*(arr))); \
        } \
        (arr)[(count)++] = (val); \
    } while (0)

void predeclare_binders(ASTNode* pat);

// ── `match` LOWERING ENGINE ──────────────────────────────────────────────────
bool pattern_covers_all(ASTNode* pat) {
    if (!pat) return true;
    if (pat->type == AST_IDENT) return true;
    if (pat->type == AST_DEREF && pat->unary && pat->unary->type == AST_IDENT) return true;
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

static ASTNode* make_tag_read(ASTNode* scrut) {
    ASTNode* addr = new_node(AST_ADDR); addr->unary = scrut;
    Type* u32t = (Type*)calloc(1, sizeof(Type)); u32t->cls = TYPE_PRIMITIVE; u32t->primitive = PRIM_U32;
    Type* u32p = (Type*)calloc(1, sizeof(Type)); u32p->cls = TYPE_POINTER; u32p->pointer_base = u32t;
    ASTNode* c = new_node(AST_CAST); c->cast.target_type = u32p; c->cast.expr = addr;
    ASTNode* d = new_node(AST_DEREF); d->unary = c;
    return d;
}

ASTNode* make_tag_eq(ASTNode* scrut, int idx) {
    ASTNode* tag = make_tag_read(scrut);
    ASTNode* lit = new_node(AST_INT_LITERAL); lit->lit_kind = LIT_INT; lit->int_value = (uint64_t)idx;
    ASTNode* eq = new_node(AST_EQ); eq->binary.left = tag; eq->binary.right = lit;
    return eq;
}

void compile_pattern(ASTNode* pat, ASTNode* scrut, Type* scrut_type, ASTNode** out_cond, ASTNode*** out_decls, size_t* decl_count, size_t* decl_cap) {
    if (pat->type == AST_IDENT) {
        Symbol* pre = NULL;
        for (size_t i = 0; i < s_symtable->count; i++) {
            Symbol* e = s_symtable->symbols[i];
            if (e->name_len == pat->ident.name_len &&
                strncmp(e->name, pat->ident.name, pat->ident.name_len) == 0 &&
                e->type == NULL) { pre = e; break; }
        }
        if (pre) {
            pre->type = scrut_type;
            ASTNode* decl = make_decl_stmt(scrut_type, pat->ident.name, pat->ident.name_len, pre, scrut);
            DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
            return;
        }
        SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
        Symbol* sym = SymTable_Add(s_symtable, pat->ident.name, pat->ident.name_len, scrut_type, kind);
        ASTNode* decl = make_decl_stmt(scrut_type, pat->ident.name, pat->ident.name_len, sym, scrut);
        DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
        return;
    }

    if (pat->type == AST_DEREF && pat->unary && pat->unary->type == AST_IDENT) {
        const char* nm = pat->unary->ident.name;
        size_t nml = pat->unary->ident.name_len;
        Type* ptr_t = make_pointer_type(scrut_type);
        ASTNode* addr = new_node(AST_ADDR); addr->unary = scrut;
        Symbol* pre = NULL;
        for (size_t i = 0; i < s_symtable->count; i++) {
            Symbol* e = s_symtable->symbols[i];
            if (e->name_len == nml && strncmp(e->name, nm, nml) == 0 && e->type == NULL) { pre = e; break; }
        }
        if (pre) {
            pre->type = ptr_t;
            ASTNode* decl = make_decl_stmt(ptr_t, nm, nml, pre, addr);
            DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
        } else {
            SymbolKind kind = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
            Symbol* sym = SymTable_Add(s_symtable, nm, nml, ptr_t, kind);
            ASTNode* decl = make_decl_stmt(ptr_t, nm, nml, sym, addr);
            DA_PUSH(*out_decls, *decl_count, *decl_cap, decl);
        }
        return;
    }

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

typedef struct {
    ASTNode* outer;
    ASTNode* chain_tail_if;
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

static bool matchchain_add_arm(MatchChain* mc, ASTNode* ifn, ASTNode* armblk, bool is_terminal) {
    ASTNode* to_link = is_terminal ? armblk : ifn;
    if (!mc->chain_tail_if) matchchain_push_stmt(mc, to_link);
    else mc->chain_tail_if->if_stmt.false_block = to_link;
    mc->chain_tail_if = is_terminal ? NULL : ifn;
    return !is_terminal;
}

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

    bool scrut_is_lvalue = base_is_lvalue(scrut);
    Type* msym_type = scrut_is_lvalue ? make_pointer_type(st) : st;
    Symbol* msym = SymTable_Add(s_symtable, mname, mn, msym_type, kind);

    MatchChain mc = matchchain_begin();
    ASTNode* outer = mc.outer;
    if (node->match_stmt.is_unpack) outer->block.transparent = true;
    if (scrut_is_lvalue) {
        ASTNode* addr_of_scrut = new_node(AST_ADDR); addr_of_scrut->unary = scrut;
        matchchain_push_stmt(&mc, make_decl_stmt(msym_type, mname, mn, msym, addr_of_scrut));
    } else {
        matchchain_push_stmt(&mc, make_decl_stmt(st, mname, mn, msym, scrut));
    }
    #define SREF() ({ ASTNode* r = new_node(AST_IDENT); r->ident.name = mname; \
                      r->ident.name_len = mn; r->ident.sym = msym; \
                      scrut_is_lvalue ? ({ ASTNode* d = new_node(AST_DEREF); d->unary = r; d; }) : r; })

    StructDef* enum_sd = (st->cls == TYPE_STRUCT) ? Struct_Find(st->struct_name) : NULL;
    bool is_enum_match = enum_sd && enum_sd->is_enum;
    bool* enum_covered = is_enum_match ? calloc(enum_sd->field_count, sizeof(bool)) : NULL;
    bool covered_true = false, covered_false = false, has_wildcard = false;
    uint8_t** seen = NULL; size_t seen_count = 0, seen_cap = 0;

    for (size_t a = 0; a < narms; a++) {
        ASTNode* pat  = node->match_stmt.arm_patterns[a];
        ASTNode* body = node->match_stmt.arm_bodies[a];
        SymbolTable* arm_scope = node->match_stmt.arm_scopes[a];
        bool is_wildcard = (pat == NULL);

        if (!is_wildcard && Type_IsAggregate(st)) resolve_brace_literal(pat, st);

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

        ASTNode* proj_scrut = NULL; Type* proj_type = st;
        if (node->match_stmt.is_unpack && !is_wildcard) {
            proj_scrut = SREF();
            if (st->cls == TYPE_POINTER && st->pointer_base && pat->type != AST_IDENT &&
                Type_IsAggregate(st->pointer_base)) {
                ASTNode* d = new_node(AST_DEREF); d->unary = proj_scrut; proj_scrut = d;
                proj_type = st->pointer_base;
            }
            if (Type_IsAggregate(proj_type)) resolve_brace_literal(pat, proj_type);
        }

        ASTNode* cond = NULL;
        if (!is_wildcard) {
            ASTNode** decls = NULL; size_t dc = 0, dcap = 0;
            SymbolTable* save = s_symtable;
            s_symtable = arm_scope;
            compile_pattern(pat, proj_scrut ? proj_scrut : SREF(), proj_type, &cond, &decls, &dc, &dcap);
            s_symtable = save;
            if (node->match_stmt.is_unpack) {
                for (size_t i = 0; i < dc; i++) matchchain_push_stmt(&mc, decls[i]);
                if (decls) free(decls);
                continue;
            }
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
            uint8_t* bytes = (uint8_t*)calloc(1, sz);
            if (ConstEval_Bytes(pat, bytes, sz)) {
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

    if (!has_wildcard && !node->match_stmt.is_unpack) {
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

ASTNode* parse_match_type(ASTNode* scrut) {
    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' to begin match arms");
    advance();

    MatchChain mc = matchchain_begin();
    ASTNode* outer = mc.outer;
    bool has_wildcard = false;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        bool is_else = (s_curr.type == TOK_ELSE);

        Type* pattern = NULL;
        size_t wc_start = s_match_wildcard_count;

        if (is_else) {
            advance();
        } else {
            bool prev = s_in_match_pattern;
            s_in_match_pattern = true;
            bool prev_pt = s_pattern_types_ok; s_pattern_types_ok = true;
            pattern = parse_type();
            s_in_match_pattern = prev;
            s_pattern_types_ok = prev_pt;
            if (!pattern) parse_error("Expected a type pattern in match arm");
        }

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
                bool is_val = (s_match_wc_is_size && s_match_wc_is_size[wc_start + i]);
                merged_kinds[prev_tpc + i] = is_val ? Type_MakePrim(PRIM_U32) : NULL;
            }
            s_type_params = merged_names;
            s_param_kinds = merged_kinds;
            s_type_param_count = total;
        }

        ASTNode* armblk = parse_block_body();

        s_type_params = prev_tp;
        s_param_kinds = prev_pk;
        s_type_param_count = prev_tpc;
        s_match_wildcard_count = wc_start;

        if (is_else) {
            has_wildcard = true;
            matchchain_add_arm(&mc, NULL, armblk, true);
            break;
        }

        ASTNode* placeholder = new_node(AST_INT_LITERAL);
        placeholder->lit_kind = LIT_BOOL; placeholder->int_value = 0;
        ASTNode* ifn = new_node(AST_IF);
        ifn->if_stmt.condition = placeholder;
        ifn->if_stmt.true_block = armblk;
        ifn->if_stmt.false_block = NULL;
        ifn->if_stmt.reflect_pattern = pattern;
        ifn->if_stmt.reflect_scrutinee = scrut->sizeof_expr.type;

        matchchain_add_arm(&mc, ifn, armblk, false);
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end match");
    advance();

    (void)has_wildcard;
    return outer;
}

ASTNode* parse_unpack(void) {
    advance();

    ASTNode* pat = parse_expr_prec(2);

    if (s_curr.type != TOK_EQ) parse_error("Expected '=' after unpack pattern");
    advance();

    ASTNode* scrut = parse_expr_prec(0);

    if (!pattern_covers_all(pat))
        parse_error("unpack pattern must always match -- no literal-pinned fields "
                     "(e.g. `{.x=3, .y=py}`) or enum-variant patterns are allowed here; "
                     "use `match` if the pattern can fail");
    predeclare_binders(pat);
    ASTNode* node = new_node(AST_MATCH);
    node->match_stmt.scrutinee = scrut;
    node->match_stmt.is_type_match = false;
    node->match_stmt.is_unpack = true;
    node->match_stmt.arm_patterns = malloc(sizeof(ASTNode*));
    node->match_stmt.arm_bodies   = malloc(sizeof(ASTNode*));
    node->match_stmt.arm_scopes   = malloc(sizeof(SymbolTable*));
    node->match_stmt.arm_patterns[0] = pat;
    ASTNode* empty = new_node(AST_BLOCK);
    empty->block.capacity = 0; empty->block.count = 0; empty->block.statements = NULL;
    node->match_stmt.arm_bodies[0] = empty;
    node->match_stmt.arm_scopes[0] = s_symtable;
    node->match_stmt.arm_count = 1;
    if (s_curr.type == TOK_SEMI) advance();
    return node;
}

void predeclare_binders(ASTNode* pat) {
    if (!pat) return;
    if (pat->type == AST_DEREF && pat->unary && pat->unary->type == AST_IDENT) {
        bool here = false;
        for (size_t i = 0; i < s_symtable->count; i++) {
            Symbol* e = s_symtable->symbols[i];
            if (e->name_len == pat->unary->ident.name_len &&
                strncmp(e->name, pat->unary->ident.name, pat->unary->ident.name_len) == 0) { here = true; break; }
        }
        if (!here) {
            SymbolKind k = s_symtable->is_function_scope ? SYM_LOCAL : SYM_GLOBAL;
            SymTable_Add(s_symtable, pat->unary->ident.name, pat->unary->ident.name_len, NULL, k);
        }
        return;
    }
    if (pat->type == AST_IDENT) {
        bool here = false;
        for (size_t i = 0; i < s_symtable->count; i++) {
            Symbol* e = s_symtable->symbols[i];
            if (e->name_len == pat->ident.name_len &&
                strncmp(e->name, pat->ident.name, pat->ident.name_len) == 0) { here = true; break; }
        }
        if (!here) {
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
}

ASTNode* parse_match(void) {
    advance();

    if (s_curr.type == TOK_LPAREN) {
        LexerState msave; Lexer_Save(&msave);
        Token cur_save = s_curr;
        Type* tt = parse_type();
        if (tt && s_curr.type == TOK_LBRACE) {
            ASTNode* te = new_node(AST_TYPE_EXPR);
            te->sizeof_expr.type = tt;
            return parse_match_type(te);
        }
        Lexer_Restore(&msave); s_curr = cur_save;
    }

    ASTNode* scrut = parse_expr_prec(0);

    if (scrut->type == AST_TYPE_EXPR) return parse_match_type(scrut);

    if (s_curr.type != TOK_LBRACE) parse_error("Expected '{' to begin match arms");
    advance();

    ASTNode* node = new_node(AST_MATCH);
    node->match_stmt.scrutinee = scrut;
    node->match_stmt.is_type_match = false;
    ASTNode** pats = NULL; size_t np = 0, pcap = 0;
    ASTNode** bodies = NULL; size_t nb = 0, bcap = 0;
    SymbolTable** scopes = NULL; size_t ns = 0, scap = 0;

    while (s_curr.type != TOK_RBRACE && s_curr.type != TOK_EOF) {
        ASTNode* pat = NULL;
        if (s_curr.type == TOK_ELSE) {
            advance();
        } else if (s_curr.type == TOK_DOT) {
            advance();
            pat = parse_enum_variant_after_dot();
        } else {
            pat = parse_expr_prec(0);
        }

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

        if (!pat) break;
    }
    if (s_curr.type != TOK_RBRACE) parse_error("Expected '}' to end match");
    advance();

    node->match_stmt.arm_patterns = pats;
    node->match_stmt.arm_bodies = bodies;
    node->match_stmt.arm_scopes = scopes;
    node->match_stmt.arm_count = np;
    return node;
}
