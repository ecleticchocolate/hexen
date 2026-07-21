//@ expect val 730
// Explicit value slots at BOTH levels of nested wildcard heads -- the fully
// nested form is now expressible: N pinned at outer, P pinned at inner.
struct Two[u32 A, X] { X v }
struct Vec[T, u32 N] { T[N] e }
fn f[S]() i32 {
    match S {
        struct M[u32 N, struct K[E, u32 P]] { return (i32)N * 100 + (i32)P }
        else { return -1 }
    }
    return -2
}
fn main() i32 { return f[Two[7, Vec[i32, 30]]]() }
