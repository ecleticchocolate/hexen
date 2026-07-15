//@ expect val 0
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] o = .None
    match o {
        .Some{v} { return (i32) v }
        .None { return 0 }
    }
    return -1
}
