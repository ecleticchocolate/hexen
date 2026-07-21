//@ expect val 3218
// `Ts...` works on every declaration form that takes a generic list. The
// carrier is always STRUCT-kinded regardless of the enclosing declaration's
// kind -- the `union`/`enum` keyword describes the container being declared,
// not the carrier the bracket arguments were bundled into. A union therefore
// still overlaps its own fields (carrier vs u32), and an enum still tags.
union U[Ts...] { Ts a  u32 b }
enum  E[Ts...] { Ts Payload  None }
struct S[Ts...] { Ts f  u32 n }
fn nf[T]() u32 { match T { struct { H; Rest... } { return 1 + nf[Rest]() } else { return 0 } } }
fn ucount[X]() u32 { match X { U[H, Rest...] { return 1 + nf[Rest]() } else { return 0 } } }
fn carrier_kind[X]() u32 {
    match X {
        U[C] { match C { struct { A; B } { return 1 } union { A; B } { return 2 } else { return 0 } } }
        else { return 9 }
    }
}
fn main() i32 {
    u32 r = ucount[U[i32,u8,f64]]() * 1000        // 3000
    r = r + (u32)sizeof(U[i32,u8]) * 25           // 8 * 25 = 200
    r = r + (u32)sizeof(S[i32,u8])                // 12
    r = r + carrier_kind[U[i32,u8]]() * 6         // 1 * 6 = 6
    return (i32)r
}
