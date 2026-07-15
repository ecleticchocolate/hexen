//@ expect val 5
enum Option[T] { T Some  None }
fn main() i32 {
    Option[Option[u32]] oo = .Some{ .Some{5} }
    match oo {
        .Some{inner} {
            match inner {
                .Some{v} { return (i32) v }
                .None { return -2 }
            }
        }
        .None { return -1 }
    }
    return -3
}
