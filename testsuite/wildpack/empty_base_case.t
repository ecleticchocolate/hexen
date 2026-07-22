//@ expect val 10
// `struct M[]` -- the empty-arg base case a peeling recursion needs to
// terminate on, now accepted under a wildcard head (previously a parse
// error). Mirrors the already-tested named-head `Def[]` base case.
struct Def[Ts...] { Ts field  u32 n }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn arity[X]() u32 { match X { struct M[E] { return nf[E]() } else { return 999 } } }
fn main() i32 {
    u32 s = arity[Def[]]()
    s = s + arity[Def[i32]]()
    s = s + arity[Def[i32,u8]]()
    s = s + arity[Def[i32,u8,f64]]()
    s = s + arity[Def[i32,u8,f64,u16]]()
    return (i32)s
}
