//@ expect val 1
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] a = .Some{42}
    Option[u32] b = .Some{42}
    return (i32)(a == b)
}
