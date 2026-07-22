//@ expect val 4
// The core fix: `struct M[H, Rest...]` under a WILDCARD head now unifies
// against a real instantiation instead of hard-erroring at parse time. H
// binds to the FIRST individual type argument (not the whole bundle).
extern fn printf(u8* fmt, ...) i32
struct Def2[Ts...] { Ts field }
fn f[S]() i32 {
    match S {
        struct M[H, Rest...] { return (i32)sizeof(H) }
        else { return -100 }
    }
    return -2
}
fn main() i32 { return f[Def2[i32, u8]]() }
