//@ expect val 12
struct S { i32 a  u8 b  u8 c }
struct W { i32 z  u8 b  u8 c }
fn probe[T]() i32 {
    match T { struct { i32 a; Rest... } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]()*10 + probe[W]() }
