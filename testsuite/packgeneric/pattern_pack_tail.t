//@ expect val 13
// `Def[H, Rest...]` is the bracket-position spelling of `struct { H; Rest... }`
// -- exactly the same construct, because the carrier the header built IS an
// anonymous struct. H binds the first type argument, Rest rebundles the
// remainder, and `Def[Rest]` re-applies the generic to that tail, so a walk
// over a generic's own argument list recurses to the empty base case.
//
// The lone-TYPE_PARAM pass-through is what makes the recursion terminate:
// without it `Def[Rest]` would bundle an already-bundled carrier into
// Def[struct{struct{...}}], adding a layer per step, and `Def[]` would never
// be reached (the compiler stack-overflowed instead).
struct Def[Ts...] { Ts field  u32 n }
fn walk[X]() u32 {
    match X {
        Def[]           { return 0 }
        Def[H, Rest...] { return (u32)sizeof(H) + walk[Def[Rest]]() }
        else            { return 9999 }
    }
}
fn main() i32 { return (i32)walk[Def[i32, u8, f64]]() }  // 4 + 1 + 8
