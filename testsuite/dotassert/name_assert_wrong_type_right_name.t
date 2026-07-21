//@ expect val 2
struct S { i32 a  u8 b }
fn probe[T]() i32 {
    match T { struct { i32 a  u32 b } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
