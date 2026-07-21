//@ expect val 0
// The explicit value slot still MATCHES for real: a wrong head falls through.
struct Vec[T, u32 N] { T[N] e }
struct Box[T] { T v }
fn f[S]() i32 { match S { struct M[E, u32 N] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Box[i32]]() }
