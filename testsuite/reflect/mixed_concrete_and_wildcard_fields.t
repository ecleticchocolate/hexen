//@ expect val 8
struct S { i32 x  u64 y }
fn main() i32 {
    match S {
        struct { i32 a  E b } { return (i32)sizeof(E) }
        else { return 99 }
    }
}
