//@ expect val 6
fn fields3[T](T x) u32 {
    match T {
        struct { A a  Rest... r } { return (u32)sizeof(A) + (u32)sizeof(Rest) }
        else { return 0 }
    }
}
fn main() i32 {
    struct { i32 p  u8 q  u8 z } v
    return (i32)fields3(v)
}
