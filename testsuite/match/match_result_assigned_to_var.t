//@ expect val 42
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] o = .Some{42}
    i32 result = 0
    match o {
        .Some{v} { result = (i32) v }
        .None { result = -1 }
    }
    return result
}
