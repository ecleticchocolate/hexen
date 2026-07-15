//@ expect val 5
fn fields2[T](T x) u32 {
    match T {
        struct { A a  B b } { return sizeof(A) + sizeof(B) }
        else { return 0 }
    }
}
fn main() i32 {
    struct { i32 p  u8 q } v
    return (i32)fields2(v)
}
