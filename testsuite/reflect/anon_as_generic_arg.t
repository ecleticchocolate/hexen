//@ expect val 7
struct Box[T]{ T v }
fn main() i32 {
    Box[struct { i32 x }] b
    b.v.x = 7
    return (i32)b.v.x
}
