//@ expect val 5
struct Holder[T, u32 N] { T[N] items }
fn main() i32 {
    Holder[struct { i32 a  i32 b }, 3] h
    h.items[0].a = 5
    return (i32)h.items[0].a
}
