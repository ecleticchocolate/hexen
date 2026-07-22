//@ expect val 4
// H, Rest... in one match arm, immediately feeding Rest into the existing
// field-pack recursion in the SAME function -- the direct wildcard-head
// one-shot peel, not routed through arity[E] first.
struct Def[Ts...] { Ts field  u32 n }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn arity2[X]() u32 {
    match X {
        struct M[H, Rest...] { return 1 + nf[Rest]() }
        struct M[] { return 0 }
        else { return 999 }
    }
}
fn main() i32 { return (i32)arity2[Def[i32,u8,f64,u16]]() }
