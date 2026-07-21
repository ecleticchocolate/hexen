//@ expect val 1
// An argument is an ordinary type production, parsed by the shared entry point,
// so postfix composes for free. Hand-rolling the wildcard case here dropped the
// postfix loop and broke `struct M[E*]` / `struct M[E[N]]`.
struct Box[T] { T v }
fn p[T]() i32 { match T { struct M[E*] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Box[u8*]]() }
