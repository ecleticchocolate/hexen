//@ expect val 1233
// A match arm may be written in the SAME sugar as a use site: `Def[i32, u8]`
// as an arm means exactly the type `Def[i32, u8]` names elsewhere. The single
// carve-out is a lone WILDCARD (`Def[E]`), which binds the already-bundled
// struct so one arm still covers every arity -- wrapping it into struct{E}
// would have restricted that arm to 1-arity instantiations.
struct Def[Ts...] { Ts field  u32 n }
fn which[X]() i32 {
    match X {
        Def[i32]     { return 1 }
        Def[i32, u8] { return 2 }
        Def[E]       { return 3 }
        else         { return 0 }
    }
}
fn main() i32 {
    i32 r = which[Def[i32]]() * 1000
    r = r + which[Def[i32,u8]]() * 100
    r = r + which[Def[f64]]() * 10
    r = r + which[Def[u8,u8,u8]]()
    return r
}
