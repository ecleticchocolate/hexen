//@ expect val 10
// `Ts...` -- one declaration serving every arity. The whole point: without it
// you enumerate `D2`/`D3`/`D4` aliases up front and cap yourself at whatever
// arity you guessed, the same way C without varargs would need printf2/printf3.
struct Def[Ts...] { Ts field  u32 n }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn arity[X]() u32 { match X { Def[E] { return nf[E]() } else { return 999 } } }
fn main() i32 {
    // 0 + 1 + 2 + 3 + 4 = 10, from ONE Def declaration and ONE match arm
    u32 s = arity[Def[]]()
    s = s + arity[Def[i32]]()
    s = s + arity[Def[i32,u8]]()
    s = s + arity[Def[i32,u8,f64]]()
    s = s + arity[Def[i32,u8,f64,u16]]()
    return (i32)s
}
