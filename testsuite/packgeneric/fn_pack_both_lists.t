//@ expect val 22
// The bracket pack and the paren pack coexist and agree: bundling two types
// explicitly and bundling two values whose types are those types produce the
// same carrier, so both report arity 2.
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn viaTypes[Ts...]() u32 { return nf[Ts]() }
fn viaVals[T](T... args) u32 { return nf[T]() }
fn main() i32 {
    u32 r = viaTypes[i32, u8]() * 10   // 20
    r = r + viaVals(1, (u8)2)          // 2
    return (i32)r
}
