//@ expect val 318
// H binds the real head and Rest the real tail -- verified by measuring both
// independently, and by peeling the SAME argument list from both ends.
struct Def[Ts...] { Ts field  u32 n }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn count[X]() u32 { match X { Def[H, Rest...] { return 1 + nf[Rest]() } else { return 0 } } }
fn head[X]()  u32 { match X { Def[H, Rest...] { return (u32)sizeof(H) } else { return 0 } } }
fn main() i32 {
    u32 r = count[Def[i32,u8,f64]]() * 100   // 300
    r = r + head[Def[f64,u8]]() * 2          // 16
    r = r + head[Def[u8,f64]]() * 2          // 2
    return (i32)r
}
