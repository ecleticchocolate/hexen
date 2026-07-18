//@ expect val 442186
// A tiny Lisp in Torrent: READS real source text and evaluates it.
//   (+ (* 6 7) (- 10 8))  ->  44
// Exercises: char-by-char string scanning (the reader), heap cons cells, a
// tagged-union value type, recursive eval, match dispatch — a real program.

struct Cell { Val* a  Val* b }

// A value is an integer, an operator symbol (encoded by an op code), a cons cell,
// or nil. Operators: 1=+, 2=-, 3=*  (the reader maps the char to this code).
enum Val { i64 Int  i64 Op  Cell Cons  i64 Nil }

fn mk_int(i64 n) Val* {
    Val* v = new Val
    *v = .Int(n)
    return v
}
fn mk_op(i64 o) Val* {
    Val* v = new Val
    *v = .Op(o)
    return v
}
fn mk_nil() Val* {
    Val* v = new Val
    *v = .Nil(0)
    return v
}

fn cons(Val* a, Val* b) Val* {
    Val* v = new Val
    *v = .Cons( {.a = a, .b = b} )
    return v
}

fn car(Val* v) Val* { match *v { .Cons(c) { return c.a }  .Int(n) { return v }  .Op(o) { return v }  .Nil(x) { return v } }  return v }
fn cdr(Val* v) Val* { match *v { .Cons(c) { return c.b }  .Int(n) { return v }  .Op(o) { return v }  .Nil(x) { return v } }  return v }

fn is_nil(Val* v) bool { match *v { .Nil(x) { return true }  .Int(n) { return false }  .Op(o) { return false }  .Cons(c) { return false } }  return false }

// ─── Reader ──────────────────────────────────────────────────────────────────
// Parses from a NUL-terminated u8* buffer. A global cursor walks the string.
u8* g_src        // set by run() (null-global init is a known limitation)
u32 g_pos = 0

fn peek() u8 { return g_src[g_pos] }
fn next() u8 { u8 c = g_src[g_pos]  g_pos = g_pos + 1  return c }

fn skip_ws() {
    while peek() == 32 { g_pos = g_pos + 1 }   // 32 = space
}

fn is_digit(u8 c) bool { return c >= 48 && c <= 57 }   // '0'..'9'

// Read one s-expression: a number, an operator, or a parenthesized list.
fn read() Val* {
    skip_ws()
    u8 c = peek()
    if c == 40 {                       // '(' -> read a list
        g_pos = g_pos + 1              // consume '('
        return read_list()
    }
    if c == 43 { g_pos = g_pos + 1  return mk_op(1) }   // '+'
    if c == 45 { g_pos = g_pos + 1  return mk_op(2) }   // '-'
    if c == 42 { g_pos = g_pos + 1  return mk_op(3) }   // '*'
    if is_digit(c) {
        i64 n = 0
        while is_digit(peek()) { n = n * 10 + (i64)(next() - 48) }
        return mk_int(n)
    }
    return mk_nil()
}

// Read list elements until ')'.
fn read_list() Val* {
    skip_ws()
    if peek() == 41 {                 // ')' -> end of list
        g_pos = g_pos + 1
        return mk_nil()
    }
    Val* head = read()
    Val* rest = read_list()
    return cons(head, rest)
}

// ─── Evaluator ───────────────────────────────────────────────────────────────
fn op_of(Val* v) i64 { match *v { .Op(o) { return o }  .Int(n) { return 0 }  .Cons(c) { return 0 }  .Nil(x) { return 0 } }  return 0 }

fn eval(Val* e) i64 {
    match *e {
        .Int(n) { return n }
        .Op(o) { return 0 }
        .Nil(x) { return 0 }
        .Cons(c) {
            i64 op = op_of(car(e))     // operator symbol
            Val* rest = cdr(e)
            i64 a = eval(car(rest))
            i64 b = eval(car(cdr(rest)))
            if op == 1 { return a + b }
            if op == 2 { return a - b }
            if op == 3 { return a * b }
            return 0
        }
    }
    return 0
}

fn run(u8* program) i64 {
    g_src = program
    g_pos = 0
    Val* expr = read()
    return eval(expr)
}

fn main() i32 {
    // Parse and evaluate REAL Lisp text:
    i64 r1 = run("(+ (* 6 7) (- 10 8))")   // 42 + 2  = 44
    i64 r2 = run("(* (+ 1 2) (+ 3 4))")     // 3 * 7   = 21
    i64 r3 = run("(- 100 (* 7 (+ 1 1)))")   // 100 - 14 = 86
    return (i32)(r1 * 10000 + r2 * 100 + r3)   // 44 21 86 -> 442186
}
