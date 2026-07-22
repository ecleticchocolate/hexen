//@ expect val 42
// Dual syntax composing: a const-generic value slot (`u32 N`, the explicit
// pin/bind form) sitting BEFORE the pack tail, under a wildcard head. N must
// bind to the real value and be usable as a value in the arm body.
struct Def[u32 N, Ts...] { Ts field  u32 n }
fn f[S]() i32 {
    match S {
        struct M[u32 N, Ts...] { return (i32)N }
        else { return -100 }
    }
    return -2
}
fn main() i32 { return f[Def[42, i32, u8, f64]]() }
