#include "compiler.h"
#include <stdio.h>
#include <string.h>

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

static void print_string(const char* s, size_t len) {
    printf("\"");
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '"') printf("\\\"");
        else if (s[i] == '\\') printf("\\\\");
        else if (s[i] == '\n') printf("\\n");
        else if (s[i] == '\t') printf("\\t");
        else printf("%c", s[i]);
    }
    printf("\"");
}

void AST_Dump(struct ASTNode* node, int depth) {
    if (!node) {
        printf("null");
        return;
    }

    printf("{\n");
    print_indent(depth + 1);

    switch (node->type) {
        case AST_INT_LITERAL:
            printf("\"type\": \"IntLiteral\",\n");
            print_indent(depth + 1);
            printf("\"value\": %lu\n", (unsigned long)node->int_value);
            break;
        case AST_IDENT:
            printf("\"type\": \"Ident\",\n");
            print_indent(depth + 1);
            printf("\"name\": "); print_string(node->ident.name, node->ident.name_len); printf("\n");
            break;
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR: case AST_SHL: case AST_SHR:
        case AST_EQ: case AST_NEQ: case AST_LT: case AST_GT: case AST_LTE: case AST_GTE:
        case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_ASSIGN: {
            const char* op = "";
            if (node->type == AST_ADD) op = "+";
            else if (node->type == AST_SUB) op = "-";
            else if (node->type == AST_MUL) op = "*";
            else if (node->type == AST_DIV) op = "/";
            else if (node->type == AST_MOD) op = "%";
            else if (node->type == AST_BIT_AND) op = "&";
            else if (node->type == AST_BIT_OR) op = "|";
            else if (node->type == AST_BIT_XOR) op = "^";
            else if (node->type == AST_SHL) op = "<<";
            else if (node->type == AST_SHR) op = ">>";
            else if (node->type == AST_EQ) op = "==";
            else if (node->type == AST_NEQ) op = "!=";
            else if (node->type == AST_LT) op = "<";
            else if (node->type == AST_GT) op = ">";
            else if (node->type == AST_LTE) op = "<=";
            else if (node->type == AST_GTE) op = ">=";
            else if (node->type == AST_LOGICAL_AND) op = "&&";
            else if (node->type == AST_LOGICAL_OR) op = "||";
            else if (node->type == AST_ASSIGN) op = "=";

            printf("\"type\": \"Binary\",\n");
            print_indent(depth + 1);
            printf("\"op\": \"%s\",\n", op);
            print_indent(depth + 1);
            printf("\"left\": "); AST_Dump(node->binary.left, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"right\": "); AST_Dump(node->binary.right, depth + 1); printf("\n");
            break;
        }
        case AST_LOGICAL_NOT: case AST_BIT_NOT: case AST_DEREF: case AST_ADDR: case AST_TYPE_EXPR: {
            const char* op = "";
            if (node->type == AST_LOGICAL_NOT) op = "!";
            else if (node->type == AST_BIT_NOT) op = "~";
            else if (node->type == AST_DEREF) op = "*";
            else if (node->type == AST_ADDR) op = "&";
            else if (node->type == AST_TYPE_EXPR) op = "type";
            printf("\"type\": \"Unary\",\n");
            print_indent(depth + 1);
            printf("\"op\": \"%s\",\n", op);
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->unary, depth + 1); printf("\n");
            break;
        }
        case AST_DECLARATION:
            printf("\"type\": \"Decl\",\n");
            print_indent(depth + 1);
            printf("\"name\": "); print_string(node->decl.name, node->decl.name_len); printf(",\n");
            print_indent(depth + 1);
            printf("\"init\": "); AST_Dump(node->decl.init_expr, depth + 1); printf("\n");
            break;
        case AST_BLOCK:
            printf("\"type\": \"Block\",\n");
            print_indent(depth + 1);
            printf("\"statements\": [\n");
            for (size_t i = 0; i < node->block.count; i++) {
                print_indent(depth + 2);
                AST_Dump(node->block.statements[i], depth + 2);
                if (i < node->block.count - 1) printf(",");
                printf("\n");
            }
            print_indent(depth + 1);
            printf("]\n");
            break;
        case AST_CAST:
            printf("\"type\": \"Cast\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->cast.expr, depth + 1); printf("\n");
            break;
        case AST_IF:
            printf("\"type\": \"If\",\n");
            print_indent(depth + 1);
            printf("\"cond\": "); AST_Dump(node->if_stmt.condition, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"then\": "); AST_Dump(node->if_stmt.true_block, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"else\": "); AST_Dump(node->if_stmt.false_block, depth + 1); printf("\n");
            break;
        case AST_WHILE:
            printf("\"type\": \"While\",\n");
            print_indent(depth + 1);
            printf("\"cond\": "); AST_Dump(node->while_stmt.condition, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"body\": "); AST_Dump(node->while_stmt.body, depth + 1); printf("\n");
            break;
        case AST_BREAK: printf("\"type\": \"Break\"\n"); break;
        case AST_CONTINUE: printf("\"type\": \"Continue\"\n"); break;
        case AST_RETURN:
            printf("\"type\": \"Return\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->unary, depth + 1); printf("\n");
            break;
        case AST_DEFER:
            printf("\"type\": \"Defer\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->unary, depth + 1); printf("\n");
            break;
        case AST_FOR:
            printf("\"type\": \"For\",\n");
            print_indent(depth + 1);
            printf("\"init\": "); AST_Dump(node->for_stmt.init, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"cond\": "); AST_Dump(node->for_stmt.cond, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"incr\": "); AST_Dump(node->for_stmt.incr, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"body\": "); AST_Dump(node->for_stmt.body, depth + 1); printf("\n");
            break;
        case AST_FUNC_DECL:
            printf("\"type\": \"FuncDecl\",\n");
            print_indent(depth + 1);
            printf("\"name\": "); print_string(node->func_decl.name, node->func_decl.name_len); printf(",\n");
            print_indent(depth + 1);
            printf("\"body\": "); AST_Dump(node->func_decl.body, depth + 1); printf("\n");
            break;
        case AST_CALL:
            printf("\"type\": \"Call\",\n");
            print_indent(depth + 1);
            printf("\"target\": "); AST_Dump(node->call.target_expr, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"args\": [\n");
            for (size_t i = 0; i < node->call.arg_count; i++) {
                print_indent(depth + 2);
                AST_Dump(node->call.args[i], depth + 2);
                if (i < node->call.arg_count - 1) printf(",");
                printf("\n");
            }
            print_indent(depth + 1);
            printf("]\n");
            break;
        case AST_STRUCT_DECL: printf("\"type\": \"StructDecl\"\n"); break;
        case AST_FIELD:
            printf("\"type\": \"Field\",\n");
            print_indent(depth + 1);
            printf("\"name\": "); print_string(node->field.field_name, node->field.field_name_len); printf(",\n");
            print_indent(depth + 1);
            printf("\"base\": "); AST_Dump(node->field.base, depth + 1); printf("\n");
            break;
        case AST_STRUCT_LITERAL:
            printf("\"type\": \"StructLit\",\n");
            print_indent(depth + 1);
            printf("\"values\": [\n");
            for (size_t i = 0; i < node->struct_lit.count; i++) {
                print_indent(depth + 2);
                AST_Dump(node->struct_lit.values[i], depth + 2);
                if (i < node->struct_lit.count - 1) printf(",");
                printf("\n");
            }
            print_indent(depth + 1);
            printf("]\n");
            break;
        case AST_INDEX:
            printf("\"type\": \"Index\",\n");
            print_indent(depth + 1);
            printf("\"array\": "); AST_Dump(node->index.base, depth + 1); printf(",\n");
            print_indent(depth + 1);
            printf("\"index\": "); AST_Dump(node->index.index, depth + 1); printf("\n");
            break;
        case AST_ARRAY_LITERAL:
            printf("\"type\": \"ArrayLit\",\n");
            print_indent(depth + 1);
            printf("\"elements\": [\n");
            for (size_t i = 0; i < node->array_lit.count; i++) {
                print_indent(depth + 2);
                AST_Dump(node->array_lit.values[i], depth + 2);
                if (i < node->array_lit.count - 1) printf(",");
                printf("\n");
            }
            print_indent(depth + 1);
            printf("]\n");
            break;
        case AST_SIZEOF:
            printf("\"type\": \"SizeOf\",\n");
            print_indent(depth + 1);
            printf("\"defer_expr\": "); AST_Dump(node->sizeof_expr.defer_expr, depth + 1); printf("\n");
            break;
        case AST_ALIGNOF:
            printf("\"type\": \"AlignOf\",\n");
            print_indent(depth + 1);
            printf("\"defer_expr\": "); AST_Dump(node->sizeof_expr.defer_expr, depth + 1); printf("\n");
            break;
        case AST_CONST_EXPR:
            printf("\"type\": \"ConstExpr\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->const_expr.inner, depth + 1); printf("\n");
            break;
        case AST_OFFSETOF:
            printf("\"type\": \"OffsetOf\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->field_ref_expr.index_expr, depth + 1); printf("\n");
            break;
        case AST_NAMEOF:
            printf("\"type\": \"NameOf\",\n");
            print_indent(depth + 1);
            printf("\"expr\": "); AST_Dump(node->field_ref_expr.index_expr, depth + 1); printf("\n");
            break;
        case AST_STRING:
            printf("\"type\": \"String\"\n");
            break;
        default:
            printf("\"type\": \"Unknown\", \"id\": %d\n", node->type);
            break;
    }

    print_indent(depth);
    printf("}");
}
