//@ expect val 0
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] a = .Some(42)
    Option[u32] b = .Some(7)
    return (i32)(a == b)
}
