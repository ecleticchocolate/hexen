#include "compiler.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const char* s_source = NULL;
static const char* s_filename = NULL;
static size_t s_pos = 0;

static size_t s_line_pos = 0;
static const char* s_line_start = NULL;
static int s_line = 1;

bool Lexer_NewlineBefore = false; // set true when skip_whitespace crosses a '\n'

void Lexer_Init(const char* filename, const char* source) {
    s_filename = filename;
    s_source = source;
    s_pos = 0;
    s_line_pos = 0;
    s_line_start = source;
    s_line = 1;
}

// Snapshot/restore the full scanner cursor so the parser can do bounded
// lookahead (peek past the current token, then rewind). Captures everything
// Lexer_NextToken reads or mutates.
void Lexer_Save(LexerState* st) {
    st->pos = s_pos;
    st->line_pos = s_line_pos;
    st->line_start = s_line_start;
    st->line = s_line;
    st->newline_before = Lexer_NewlineBefore;
}

void Lexer_Restore(const LexerState* st) {
    s_pos = st->pos;
    s_line_pos = st->line_pos;
    s_line_start = st->line_start;
    s_line = st->line;
    Lexer_NewlineBefore = st->newline_before;
}

static void skip_whitespace(void) {
    Lexer_NewlineBefore = false;
    while (1) {
        char c = s_source[s_pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            s_pos++;
        } else if (c == '\n') {
            s_pos++;
            Lexer_NewlineBefore = true;
        } else if (c == '/' && s_source[s_pos + 1] == '/') {
            s_pos += 2;
            while (s_source[s_pos] != '\0' && s_source[s_pos] != '\n') s_pos++;
        } else if (c == '/' && s_source[s_pos + 1] == '*') {
            s_pos += 2;
            // /* */ does not nest: first */ closes
            while (s_source[s_pos] != '\0' &&
                   !(s_source[s_pos] == '*' && s_source[s_pos + 1] == '/')) {
                s_pos++;
            }
            if (s_source[s_pos] != '\0') s_pos += 2; // consume closing */
        } else {
            break;
        }
    }
}

// Decode one (possibly escaped) character at s_pos, advancing past it.
// Returns the byte value, or -1 on a malformed escape.
static int decode_char(void) {
    if (s_source[s_pos] != '\\') {
        return (unsigned char)s_source[s_pos++];
    }
    s_pos++; // backslash
    char e = s_source[s_pos++];
    switch (e) {
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case '0':  return '\0';
        case '\\': return '\\';
        case '"':  return '"';
        case '\'': return '\'';
        case 'x': {
            int hi = s_source[s_pos], lo = s_source[s_pos + 1];
            if (!isxdigit(hi) || !isxdigit(lo)) return -1;
            int dh = (hi <= '9') ? hi - '0' : (tolower(hi) - 'a' + 10);
            int dl = (lo <= '9') ? lo - '0' : (tolower(lo) - 'a' + 10);
            s_pos += 2;
            return dh * 16 + dl;
        }
        default: return -1;
    }
}

static bool check_keyword(Token* tok, const char* kw, TokenType type) {
    size_t len = strlen(kw);
    if (tok->length == len && strncmp(tok->start, kw, len) == 0) {
        tok->type = type;
        return true;
    }
    return false;
}

Token Lexer_NextToken(void) {
    skip_whitespace();

    // Catch up line info to the start of the token
    while (s_line_pos < s_pos) {
        if (s_source[s_line_pos] == '\n') {
            s_line++;
            s_line_start = &s_source[s_line_pos + 1];
        }
        s_line_pos++;
    }

    Token tok = {0};
    tok.start = s_source + s_pos;
    tok.filename = s_filename;
    tok.line = s_line;
    tok.column = (int)(tok.start - s_line_start) + 1;

    char c = s_source[s_pos];
    if (c == '\0') {
        tok.type = TOK_EOF;
        return tok;
    }

    if (isalpha(c) || c == '_') {
        while (isalnum(s_source[s_pos]) || s_source[s_pos] == '_') {
            s_pos++;
        }
        tok.length = (s_source + s_pos) - tok.start;
        tok.type = TOK_IDENTIFIER;
        
        if (check_keyword(&tok, "u8", TOK_U8)) return tok;
        if (check_keyword(&tok, "u16", TOK_U16)) return tok;
        if (check_keyword(&tok, "u32", TOK_U32)) return tok;
        if (check_keyword(&tok, "u64", TOK_U64)) return tok;
        if (check_keyword(&tok, "i8", TOK_I8)) return tok;
        if (check_keyword(&tok, "i16", TOK_I16)) return tok;
        if (check_keyword(&tok, "i32", TOK_I32)) return tok;
        if (check_keyword(&tok, "i64", TOK_I64)) return tok;
        if (check_keyword(&tok, "bool", TOK_BOOL)) return tok;
        if (check_keyword(&tok, "f32", TOK_F32)) return tok;
        if (check_keyword(&tok, "f64", TOK_F64)) return tok;
        if (check_keyword(&tok, "void", TOK_VOID)) return tok;
        
        if (check_keyword(&tok, "if", TOK_IF)) return tok;
        if (check_keyword(&tok, "else", TOK_ELSE)) return tok;
        if (check_keyword(&tok, "while", TOK_WHILE)) return tok;
        if (check_keyword(&tok, "break", TOK_BREAK)) return tok;
        if (check_keyword(&tok, "continue", TOK_CONTINUE)) return tok;
        if (check_keyword(&tok, "return", TOK_RETURN)) return tok;
        if (check_keyword(&tok, "defer", TOK_DEFER)) return tok;
        if (check_keyword(&tok, "for", TOK_FOR)) return tok;
        if (check_keyword(&tok, "to", TOK_TO)) return tok;

        if (check_keyword(&tok, "fn", TOK_FN)) return tok;
        if (check_keyword(&tok, "new", TOK_NEW)) return tok;
        if (check_keyword(&tok, "delete", TOK_DELETE)) return tok;

        if (check_keyword(&tok, "struct", TOK_STRUCT)) return tok;
        if (check_keyword(&tok, "enum", TOK_ENUM)) return tok;
        if (check_keyword(&tok, "union", TOK_UNION)) return tok;
        if (check_keyword(&tok, "match", TOK_MATCH)) return tok;
        if (check_keyword(&tok, "unpack", TOK_UNPACK)) return tok;
        if (check_keyword(&tok, "auto", TOK_UNPACK)) return tok; // alias for `unpack`
        if (check_keyword(&tok, "extern", TOK_EXTERN)) return tok;
        if (check_keyword(&tok, "pub", TOK_PUB)) return tok;

        if (check_keyword(&tok, "const", TOK_CONST)) return tok;
        if (check_keyword(&tok, "with",  TOK_WITH))  return tok;
        if (check_keyword(&tok, "impl",  TOK_IMPL))  return tok;
        if (check_keyword(&tok, "alias", TOK_ALIAS)) return tok;
        if (check_keyword(&tok, "sizeof", TOK_SIZEOF)) return tok;
        if (check_keyword(&tok, "alignof", TOK_ALIGNOF)) return tok;
        if (check_keyword(&tok, "offsetof", TOK_OFFSETOF)) return tok;
        if (check_keyword(&tok, "nameof", TOK_NAMEOF)) return tok;
        if (check_keyword(&tok, "true", TOK_TRUE)) return tok;
        if (check_keyword(&tok, "false", TOK_FALSE)) return tok;
        if (check_keyword(&tok, "null", TOK_NULL)) return tok;

        return tok;
    }

    if (isdigit(c)) {
        tok.type = TOK_INTEGER;
        tok.int_value = 0;

        if (c == '0' && (s_source[s_pos + 1] == 'x' || s_source[s_pos + 1] == 'X')) {
            // Hex: 0x...
            s_pos += 2;
            if (!isxdigit(s_source[s_pos])) { tok.type = TOK_ERROR; return tok; }
            while (isxdigit(s_source[s_pos])) {
                char h = s_source[s_pos];
                int d = (h >= '0' && h <= '9') ? h - '0'
                      : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                      : h - 'A' + 10;
                tok.int_value = tok.int_value * 16 + d;
                s_pos++;
            }
        } else if (c == '0' && (s_source[s_pos + 1] == 'b' || s_source[s_pos + 1] == 'B')) {
            // Binary: 0b...
            s_pos += 2;
            if (s_source[s_pos] != '0' && s_source[s_pos] != '1') { tok.type = TOK_ERROR; return tok; }
            while (s_source[s_pos] == '0' || s_source[s_pos] == '1') {
                tok.int_value = tok.int_value * 2 + (s_source[s_pos] - '0');
                s_pos++;
            }
        } else {
            // Decimal. Spec: no octal, so a leading 0 is just decimal digits.
            while (isdigit(s_source[s_pos])) {
                tok.int_value = tok.int_value * 10 + (s_source[s_pos] - '0');
                s_pos++;
            }
            // Float: a '.' followed by a digit, or an exponent, promotes to f64.
            // (A trailing '.' with no digit stays integer so that e.g. range or
            // member syntax isn't accidentally eaten.)
            bool is_float = false;
            if (s_source[s_pos] == '.' && isdigit((unsigned char)s_source[s_pos + 1])) {
                is_float = true;
                s_pos++;                                  // consume '.'
                while (isdigit(s_source[s_pos])) s_pos++; // fractional digits
            }
            if (s_source[s_pos] == 'e' || s_source[s_pos] == 'E') {
                size_t save = s_pos;
                s_pos++;
                if (s_source[s_pos] == '+' || s_source[s_pos] == '-') s_pos++;
                if (isdigit((unsigned char)s_source[s_pos])) {
                    is_float = true;
                    while (isdigit(s_source[s_pos])) s_pos++;
                } else {
                    s_pos = save; // not an exponent after all
                }
            }
            if (is_float) {
                tok.type = TOK_FLOAT;
                tok.float_value = strtod(tok.start, NULL);
            }
        }
        tok.length = (s_source + s_pos) - tok.start;
        return tok;
    }

    if (c == '\'') {
        // Char literal -> u8 integer. Escapes: \n \t \0 \\ \" \' \xNN
        s_pos++; // opening quote
        int val = decode_char();
        if (val < 0) { tok.type = TOK_ERROR; return tok; }
        if (s_source[s_pos] != '\'') { tok.type = TOK_ERROR; return tok; }
        s_pos++; // closing quote
        tok.type = TOK_INTEGER; // char literals desugar to u8-valued integers
        tok.int_value = (uint64_t)val;
        tok.length = (s_source + s_pos) - tok.start;
        return tok;
    }

    if (c == '"') {
        // String literal -> u8* at static, NUL-terminated bytes. Decode escapes into
        // a heap buffer that lives for the JIT'd program's lifetime; the literal
        // compiles to the address of these bytes. int_value carries that pointer.
        s_pos++; // opening quote
        size_t cap = 16, len = 0;
        char* buf = (char*)malloc(cap);
        while (s_source[s_pos] != '"' && s_source[s_pos] != '\0') {
            int ch = decode_char();
            if (ch < 0) { free(buf); tok.type = TOK_ERROR; return tok; }
            if (len + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            buf[len++] = (char)ch;
        }
        if (s_source[s_pos] != '"') { free(buf); tok.type = TOK_ERROR; return tok; }
        s_pos++; // closing quote
        buf[len] = '\0'; // NUL terminate
        tok.type = TOK_STRING;
        tok.int_value = (uint64_t)(uintptr_t)buf; // stable pointer to the bytes
        tok.length = len;
        return tok;
    }

    s_pos++;
    tok.length = 1;

    switch (c) {
        case '+':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_PLUS_EQ; }
            else { tok.type = TOK_PLUS; }
            return tok;
        case '-':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_MINUS_EQ; }
            else { tok.type = TOK_MINUS; }
            return tok;
        case '*':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_STAR_EQ; }
            else { tok.type = TOK_STAR; }
            return tok;
        case '/':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_SLASH_EQ; }
            else { tok.type = TOK_SLASH; }
            return tok;
        case '%':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_MOD_EQ; }
            else { tok.type = TOK_MOD; }
            return tok;
        case '(': tok.type = TOK_LPAREN; return tok;
        case ')': tok.type = TOK_RPAREN; return tok;
        case '{': tok.type = TOK_LBRACE; return tok;
        case '}': tok.type = TOK_RBRACE; return tok;
        case ';': tok.type = TOK_SEMI; return tok;
        case ',': tok.type = TOK_COMMA; return tok;
        case '.':
            if (s_source[s_pos] == '.' && s_source[s_pos+1] == '.') {
                s_pos += 2; tok.length += 2; tok.type = TOK_ELLIPSIS;
            } else {
                tok.type = TOK_DOT;
            }
            return tok;
        case '[': tok.type = TOK_LBRACKET; return tok;
        case ']': tok.type = TOK_RBRACKET; return tok;
        case '^':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_CARET_EQ; }
            else { tok.type = TOK_CARET; }
            return tok;
        case '&':
            if (s_source[s_pos] == '&') { s_pos++; tok.length++; tok.type = TOK_ANDAND; }
            else if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_AMP_EQ; }
            else { tok.type = TOK_AMP; }
            return tok;
        case '|':
            if (s_source[s_pos] == '|') { s_pos++; tok.length++; tok.type = TOK_OROR; }
            else if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_PIPE_EQ; }
            else { tok.type = TOK_PIPE; }
            return tok;
        case '=':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_EQEQ; }
            else { tok.type = TOK_EQ; }
            return tok;
        case '~':
            tok.type = TOK_TILDE;
            return tok;
        case '!':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_NEQ; }
            else { tok.type = TOK_BANG; }
            return tok;
        case '<':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_LTE; }
            else if (s_source[s_pos] == '<') {
                s_pos++; tok.length++;
                if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_SHL_EQ; }
                else tok.type = TOK_SHL;
            }
            else { tok.type = TOK_LT; }
            return tok;
        case '>':
            if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_GTE; }
            else if (s_source[s_pos] == '>') {
                s_pos++; tok.length++;
                if (s_source[s_pos] == '=') { s_pos++; tok.length++; tok.type = TOK_SHR_EQ; }
                else tok.type = TOK_SHR;
            }
            else { tok.type = TOK_GT; }
            return tok;
        default:
            tok.type = TOK_ERROR;
            return tok;
    }
}
