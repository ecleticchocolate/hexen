//@ expect val 0
// Soundness: an explicit value-type pin ahead of a pack tail is CHECKED, not
// trusted -- u64 must not match a real u32 slot even with a pack after it.
struct Def[u32 N, Ts...] { Ts field  u32 n }
fn f[S]() i32 { match S { struct M[u64 N, Ts...] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Def[7, i32, u8]]() }
