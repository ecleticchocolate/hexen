//@ expect val 1
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] a = .None
    Option[u32] b = .None
    return (i32)(a == b)
}
