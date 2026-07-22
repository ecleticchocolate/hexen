//@ expect val 10
// The intended composition: a wildcard-head EXTRACTS the bundle once
// (struct M[E]), then recursion continues via the already-tested
// curly-brace field-pack pattern -- same total as the named-head version in
// testsuite/packgeneric/arity_polymorphic.t, just entered through the new path.
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
