#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SymbolTable* SymTable_Create(SymbolTable* parent) {
    SymbolTable* table = (SymbolTable*)calloc(1, sizeof(SymbolTable));
    table->parent = parent;
    table->capacity = 16;
    table->symbols = (Symbol**)calloc(table->capacity, sizeof(Symbol*));
    if (parent) {
        table->current_global_offset = parent->current_global_offset;
        table->current_local_offset = parent->current_local_offset;
        table->is_function_scope = parent->is_function_scope;
    } else {
        table->current_global_offset = 0;
        table->current_local_offset = 0;
        table->is_function_scope = false;
    }
    return table;
}

Symbol* SymTable_Add(SymbolTable* table, const char* name, size_t len, Type* type, SymbolKind kind) {
    // Reject redeclaration in the SAME scope (duplicate local/param/global/fn).
    // Only this table is checked, not parents — shadowing in a nested block is
    // legitimate and stays allowed (a child table is a new scope).
    for (size_t i = 0; i < table->count; i++) {
        Symbol* e = table->symbols[i];
        if (e->name_len == len && strncmp(e->name, name, len) == 0) {
            fprintf(stderr, "Error: '%.*s' is already declared in this scope\n", (int)len, name);
            exit(1);
        }
    }
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->symbols = (Symbol**)realloc(table->symbols, table->capacity * sizeof(Symbol*));
    }
    
    Symbol* sym = (Symbol*)calloc(1, sizeof(Symbol));
    table->symbols[table->count++] = sym;
    sym->name = strndup(name, len);
    sym->name_len = len;
    sym->type = type;
    sym->kind = kind;
    sym->ce_cached_addr = -1; // not yet materialized in the comptime arena (0 is a real offset)
    
    // Allocate real storage size (structs/arrays are larger than 8 bytes),
    // rounded up to an 8-byte slot so subsequent vars stay word-aligned.
    uint64_t sz = Type_SizeOf(type);
    uint64_t slot = (sz + 7) & ~(uint64_t)7;
    if (slot == 0) slot = 8;

    if (kind == SYM_GLOBAL) {
        sym->offset = table->current_global_offset;
        table->current_global_offset += slot;
    } else if (kind == SYM_LOCAL) {
        table->current_local_offset += slot;
        sym->offset = table->current_local_offset;
    }
    // SYM_CONST gets no storage slot or offset

    return sym;
}

Symbol* SymTable_Find(SymbolTable* table, const char* name, size_t len) {
    SymbolTable* curr = table;
    while (curr) {
        for (size_t i = 0; i < curr->count; i++) {
            if (curr->symbols[i]->name_len == len && 
                strncmp(curr->symbols[i]->name, name, len) == 0) {
                return curr->symbols[i];
            }
        }
        curr = curr->parent;
    }
    return NULL;
}

