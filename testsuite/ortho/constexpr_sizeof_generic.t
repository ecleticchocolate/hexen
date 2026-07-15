//@ expect val 8
struct Pair[T, U] { T a  U b }
enum Option[T] { T Some  None }
fn main() i32 {
    // Option[u32]: tag(4) + u32(4) = 8
    // Pair[u32, bool]: u32(4) + bool(1) = 5 (+ padding to 8 = 8)
    return (i32) sizeof(Option[u32])
}
