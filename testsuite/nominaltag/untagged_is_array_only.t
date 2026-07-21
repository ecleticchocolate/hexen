//@ expect val 12
// Untagged `M[X]` keeps its old meaning: array of M sized X. Never nominal.
struct Box[T] { T v }
fn p[S]() i32 { match S { M[X] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[i32[4]]()*10 + p[Box[i32]]() }
