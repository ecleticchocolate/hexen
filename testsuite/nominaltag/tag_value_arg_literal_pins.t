//@ expect val 12
// A const-generic VALUE argument written as a literal pins against the concrete.
struct Vec[T, u32 N] { T[N] e }
fn p[S]() i32 { match S { struct M[X,30] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Vec[i32,30]]()*10 + p[Vec[i32,4]]() }
