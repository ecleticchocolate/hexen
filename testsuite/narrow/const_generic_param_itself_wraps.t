//@ expect val 4
struct S[u8 N] { i32 x }
impl S[u8 N] {
    fn f() u8 { return N + 250 }
}
fn main() i32 {
    S[10] s = {.x = 0}
    return (i32)s.f()
}
