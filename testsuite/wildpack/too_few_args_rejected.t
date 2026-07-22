//@ expect val 0
// Soundness: the fixed prefix must have enough concrete args to satisfy it,
// not just enough to avoid a crash -- three fixed slots against one concrete
// arg must fall through cleanly.
struct Def2[Ts...] { Ts field }
fn f[S]() i32 { match S { struct M[A, B, C, Rest...] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Def2[i32]]() }
