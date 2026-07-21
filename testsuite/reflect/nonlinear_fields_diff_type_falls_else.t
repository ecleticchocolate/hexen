//@ expect val 99
struct S { i32 x  u8 y }
fn main() i32 {
    match S {
        struct { A; A } { return (i32)sizeof(A) }
        else { return 99 }
    }
}
