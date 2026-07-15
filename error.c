// ─── error.c ────────────────────────────────────────────────────────────────
// Single shared diagnostic printer for the whole compiler. Before this file,
// error reporting was three uncoordinated things doing similar work slightly
// differently:
//   - parser.c's parse_error(): file:line:col + longjmp-based unwind (needed
//     because Parse_Signatures relies on a silent, swallowed longjmp -- see
//     that function's setjmp comment -- so parser errors can't just exit()).
//   - types.c's local type_error(): file:line:col + source snippet + caret,
//     but hardcoded to exit(1), and only reachable from types.c.
//   - ~75 other sites in types.c/backend_x64.c/constexpr.c: bare
//     fprintf(stderr, "Error: ...\n") with no location at all.
//
// This file is the one place that formats a diagnostic and decides what
// happens after. It does NOT own control flow: it takes a jmp_buf* (NULL for
// "always exit") so parser.c keeps its existing recoverable-error behavior
// (Parse_Signatures' pre-pass swallows errors and re-parses to report them
// properly the second time) while types.c/backend_x64.c/constexpr.c -- which
// have no recovery mechanism -- exit(1) as before.
//
// Snippet/caret needs a node's line/column/filename, which only ASTNode
// carries (Token has it too, for the parser's own pre-node errors). Both
// entry points are provided below.

#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

// ANSI codes. Bold red "error:", bold message, bold cyan caret -- matches
// the Clang/GCC convention so eyes trained on those diagnostics don't have
// to relearn anything here.
#define COL_RED_BOLD  "\033[1;31m"
#define COL_BOLD      "\033[1m"
#define COL_CYAN_BOLD "\033[1;36m"
#define COL_RESET     "\033[0m"

// Color is on only when stderr is a real terminal and NO_COLOR isn't set
// (https://no-color.org). Checked once per report() call rather than cached,
// since errors are rare and this keeps behavior correct if stderr is
// redirected mid-run (e.g. by a test harness).
static bool color_enabled(void) {
    if (getenv("NO_COLOR")) return false;
    return isatty(fileno(stderr));
}

// Re-reads the source file to grab one line for the snippet. This reopens
// the file per call, which is fine for how rarely a real error fires but is
// a known inefficiency if this is ever called in a hot loop -- the lexer
// already has the full source buffer in memory; a future version should
// take a pointer to that buffer instead of a filename and avoid the reopen
// entirely. Flagged here rather than silently left as a surprise.
static void print_snippet(const char* filename, int line, int column) {
    if (!filename || line <= 0) return;
    FILE* f = fopen(filename, "r");
    if (!f) return;
    char buf[1024];
    int cur = 0;
    bool found = false;
    while (fgets(buf, sizeof(buf), f)) {
        cur++;
        if (cur == line) { found = true; break; }
    }
    fclose(f);
    if (!found) return;

    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    bool color = color_enabled();
    fprintf(stderr, "  %s\n", buf);
    fprintf(stderr, "  ");
    for (int i = 1; i < column; i++) fputc(' ', stderr);
    if (color) fprintf(stderr, "%s^%s\n", COL_CYAN_BOLD, COL_RESET);
    else       fprintf(stderr, "^\n");
}

// Core formatter. unwind_buf == NULL means "no recovery exists here, exit
// after printing" (types.c, backend_x64.c, constexpr.c today). A non-NULL
// unwind_buf means "the caller can recover; print then longjmp back to it"
// (parser.c's two setjmp sites).
static void report(const char* filename, int line, int column, const char* msg, jmp_buf* unwind_buf) {
    if (color_enabled()) {
        fprintf(stderr, "%s%s:%d:%d:%s %serror:%s %s%s%s\n",
                COL_BOLD, filename ? filename : "<unknown>", line, column, COL_RESET,
                COL_RED_BOLD, COL_RESET,
                COL_BOLD, msg, COL_RESET);
    } else {
        fprintf(stderr, "%s:%d:%d: error: %s\n",
                filename ? filename : "<unknown>", line, column, msg);
    }
    print_snippet(filename, line, column);
    if (unwind_buf) longjmp(*unwind_buf, 1);
    exit(1);
}

// --- Public entry points -----------------------------------------------

// From a Token (parser, before/while a node exists).
void Error_AtToken(Token tok, const char* msg, jmp_buf* unwind_buf) {
    report(tok.filename, tok.line, tok.column, msg, unwind_buf);
}

// From an ASTNode (typecheck, codegen -- anywhere past parsing).
void Error_AtNode(ASTNode* node, const char* msg, jmp_buf* unwind_buf) {
    const char* filename = node ? node->filename : NULL;
    int line   = node ? node->line   : 0;
    int column = node ? node->column : 0;
    report(filename, line, column, msg, unwind_buf);
}
