//@ expect val 12
struct S { i32 a  u8 b }
struct W { i32 q  u8 b }
struct Holder[T] { T v }
impl Holder[T] {
    fn kind() i32 {
        match T { struct { i32 a  u8 b } { return 1 } else { return 2 } }
        return -1
    }
}
fn main() i32 {
    Holder[S] h1  Holder[W] h2
    return h1.kind()*10 + h2.kind()
}
