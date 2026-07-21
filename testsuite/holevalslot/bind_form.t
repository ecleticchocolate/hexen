//@ expect val 1
// The BIND form: `VT N` binds the value-TYPE to VT and the value to N. Works
// even when the value-type is itself a parameter (Vec[VT, T, VT N]). VT is
// recovered from the concrete side and is usable as a type in the body.
// 5 + 252 = 257, wrapped to u8 = 1.
struct Vec[VT, T, VT N] { T[3] e }
fn f[S]() i32 {
    match S {
        struct M[VT, E, VT N] { VT dup = N  dup = dup + 252  return (i32)dup }
        else { return -1 }
    }
    return -2
}
fn main() i32 { return f[Vec[u8, i32, 5]]() }
