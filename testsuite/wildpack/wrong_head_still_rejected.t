//@ expect val 0
// Soundness: a pack-tail pattern must still fall through to else against an
// unrelated, non-generic struct rather than matching spuriously.
struct Def2[Ts...] { Ts field }
struct Other { i32 x }
fn f[S]() i32 { match S { struct M[H, Rest...] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Other]() }
