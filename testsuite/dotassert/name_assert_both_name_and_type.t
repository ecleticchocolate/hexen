//@ expect val 12
// asserts BOTH: name must be `a` AND type must be i32
struct S { i32 a  u8 b }
struct W { i32 q  u8 b }
fn probe[T]() i32 {
    match T { struct { i32 a  u8 b } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]()*10 + probe[W]() }
