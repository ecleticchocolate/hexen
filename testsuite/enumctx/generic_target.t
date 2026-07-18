//@ expect val 42
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] o = .Some(42)
    match o { .Some(v) { return (i32) v }  .None { return -1 } }
    return -2
}
