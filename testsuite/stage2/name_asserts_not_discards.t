//@ expect val 12
// The whole point of the change: a trailing name in a pattern ASSERTS. It used
// to be discarded, so `struct { i32 zzz }` silently matched a field named `a`.
struct S { i32 a }
struct W { i32 zzz }
fn p[T]() i32 { match T { struct { i32 a } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[S]()*10 + p[W]() }
