//@ expect val 1
// Same as value_slot_before_pack, but the BIND spelling (`VT N`) instead of
// the PIN spelling (`u32 N`) -- both forms must compose with a trailing pack.
struct Def[u32 N, Ts...] { Ts field  u32 n }
fn f[S]() i32 {
    match S {
        struct M[VT N, Ts...] { VT v = N  return (i32)(v == N) }
        else { return -100 }
    }
    return -2
}
fn main() i32 { return f[Def[7, i32, u8]]() }
