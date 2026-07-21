//@ expect val 1621
// Const-generic value params compose with a pack: they bind positionally
// BEFORE it, and one arm can bind both the value AND the bundle. Nothing
// special was needed -- `Ts` is an ordinary type param, so the existing
// value-param path is untouched.
struct Buf[u32 N, Ts...] { u8[N] raw  Ts extra }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn probe[X]() u32 {
    match X {
        Buf[K, E] { return K * 100 + nf[E]() }
        else      { return 0 }
    }
}
fn main() i32 {
    Buf[8, i32, u8] b          // 8 raw + {i32,u8} padded to 8 = 16
    u32 s = probe[Buf[8,i32,u8]]()      // 802
    s = s + probe[Buf[3,i32,u8,f64]]()  // 303
    s = s + probe[Buf[5]]()             // 500
    return (i32)(s + (u32)sizeof(b))    // 802+303+500+16 = 1621
}
