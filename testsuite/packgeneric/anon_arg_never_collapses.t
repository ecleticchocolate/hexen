//@ expect val 100
// HARD INVARIANT: `Def[i32,u8,f64]` (a 3-arg application, carrier
// struct{i32,u8,f64}) and `Def[struct{i32;u8;f64}]` (a 1-arg application whose
// sole arg is anonymous, carrier struct{struct{i32,u8,f64}}) are DIFFERENT
// types and must NEVER be equated. Collapsing a lone anonymous pack argument
// into the carrier is banned outright: it would make arity depend on an
// argument's anonymity (see construction_is_not_passthrough). This test exists
// so any future change that reintroduces such a dedup fails loudly here.
struct Def[Ts...] { Ts field  u32 n }
// eq returns 1 only if A and B are the SAME type.
fn eq[A, B]() i32 { match A { B { return 1 } else { return 0 } } }
fn main() i32 {
    i32 r = 0
    // the two spellings are NOT the same type
    if eq[Def[i32, u8, f64], Def[struct{i32; u8; f64}]]() == 0 { r = r + 100 }
    // sanity: an identical spelling IS the same type (dedup of a true match)
    if eq[Def[i32, u8, f64], Def[i32, u8, f64]]() == 1 { r = r + 0 } else { r = r - 1000 }
    return r
}
