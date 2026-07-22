//@ expect val 0
// Soundness: `struct M[]` must NOT match a struct with a non-empty pack.
struct Def2[Ts...] { Ts field }
fn f[S]() i32 { match S { struct M[] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Def2[i32,u8]]() }
