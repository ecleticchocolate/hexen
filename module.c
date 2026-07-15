#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Module_Generate(const char* out_filename) {
    FILE* f = fopen(out_filename, "w");
    if (!f) {
        perror("fopen module out");
        return;
    }

    fprintf(f, "// Auto-generated Torrent Module Interface\n\n");

    // 1. Export Structs & Enums
    size_t struct_count = 0;
    StructDef** structs = Struct_GetAll(&struct_count);
    for (size_t i = 0; i < struct_count; i++) {
        StructDef* sd = structs[i];
        if (!sd->is_pub) continue;
        // Don't export auto-generated concrete instantiations, only the generic
        // template itself. generic_base is set (non-NULL) exactly for an
        // instantiation (Struct_Instantiate), so this is the real signal --
        // previously this checked for a literal '$' in the name, a naming
        // convention Struct_Instantiate stopped using (it names instantiations
        // "Box[i32]", not "Box$i32"), so every instantiation was silently
        // leaking into the exported .tmod interface.
        if (sd->generic_base) continue;

        fprintf(f, "pub %s %s", sd->is_enum ? "enum" : "struct", sd->name);
        
        if (sd->is_generic && sd->type_param_count > 0) {
            fprintf(f, "[");
            for (size_t p = 0; p < sd->type_param_count; p++) {
                fprintf(f, "%s%s", sd->type_params[p], (p + 1 < sd->type_param_count) ? ", " : "");
            }
            fprintf(f, "]");
        }
        
        fprintf(f, " {\n");
        for (size_t fidx = 0; fidx < sd->field_count; fidx++) {
            StructField* field = &sd->fields[fidx];
            fprintf(f, "    ");
            if (field->type) {
                char tbuf[256];
                Type_ToString(field->type, tbuf, sizeof(tbuf));
                fprintf(f, "%s ", tbuf);
            }
            fprintf(f, "%s", field->name);
            if (field->has_default) {
                fprintf(f, " = <default>");
            }
            fprintf(f, "\n");
        }
        fprintf(f, "}\n\n");
    }

    // 2. Export Functions and Consts
    SymbolTable* gt = Get_SymTable();
    for (size_t i = 0; i < gt->count; i++) {
        Symbol* sym = gt->symbols[i];
        if (!sym->is_pub) continue;

        if (sym->kind == SYM_FUNCTION) {
            fprintf(f, "extern fn %s(", sym->name);
            Type* t = sym->type;
            if (t && t->cls == TYPE_FUNCTION) {
                for (size_t p = 0; p < t->function.param_count; p++) {
                    // Torrent's real declaration syntax is `type name` per param, but
                    // TYPE_FUNCTION only carries param_types (no names) -- there is
                    // nowhere upstream that threads a function's parameter names onto
                    // its Type. Invented placeholder names (p0, p1, ...) below are the
                    // best this function can do without that; a real fix means adding
                    // parameter names to TYPE_FUNCTION itself, not a change confined to
                    // this file.
                    char tbuf[256];
                    Type_ToString(t->function.param_types[p], tbuf, sizeof(tbuf));
                    fprintf(f, "%s p%zu%s", tbuf, p, (p + 1 < t->function.param_count) ? ", " : "");
                }
                if (t->function.is_vararg) {
                    fprintf(f, "%s...", t->function.param_count > 0 ? ", " : "");
                }
                fprintf(f, ")");
                if (t->function.return_type) {
                    char rbuf[256];
                    Type_ToString(t->function.return_type, rbuf, sizeof(rbuf));
                    fprintf(f, " %s", rbuf);
                }
            } else {
                fprintf(f, ")");
            }
            fprintf(f, "\n");
        }
    }

    size_t const_count = 0;
    ConstDef* consts = Const_GetAll(&const_count);
    for (size_t i = 0; i < const_count; i++) {
        ConstDef* c = &consts[i];
        if (!c->is_pub) continue;
        char tbuf[256];
        Type_ToString(c->type, tbuf, sizeof(tbuf));
        if (Type_IsFloat(c->type)) {
            double d;
            memcpy(&d, &c->value, sizeof(double));
            fprintf(f, "pub const %s %s = %g\n", c->name, tbuf, d);
        } else {
            fprintf(f, "pub const %s %s = %lld\n", c->name, tbuf, (long long)c->value);
        }
    }

    fclose(f);
}
