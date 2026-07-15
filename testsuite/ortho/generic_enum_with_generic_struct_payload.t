//@ expect val 6
struct Pair[T, U] { T a  U b }
enum Option[T] { T Some  None }
fn main() i32 {
    Option[Pair[u32, bool]] o = .Some{ {.a = 5, .b = true} }
    match o {
        .Some{pr} { return (i32) pr.a + (i32) pr.b }
        .None { return -1 }
    }
    return -2
}
