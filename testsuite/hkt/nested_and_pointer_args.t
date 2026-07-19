//@ expect val 50
// The HKT argument slot in M[T] can itself be a full, composed type -- not
// just a bare ident -- since Type_Substitute's array-count-expr resolution
// (Fix B) handles both an AST_IDENT (`M[T]`, T a param name) and an
// AST_TYPE_EXPR (`M[Box[i32]]`, `M[i32*]`, any type-grammar production).
struct Box[T] { T val }
struct HKT[M, T] { M[T] data }

fn nested() i32 {
    HKT[Box, Box[i32]] w
    w.data = { .val = { .val = 42 } }
    return w.data.val.val
}

fn pointer_arg() i32 {
    HKT[Box, i32*] w
    return (i32)sizeof(w)   // 8: one pointer field
}

fn main() i32 {
    return nested() + pointer_arg()   // 42 + 8 = 50
}
