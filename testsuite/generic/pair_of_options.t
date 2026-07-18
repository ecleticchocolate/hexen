//@ expect val 11
struct Pair[T, U] { T a  U b }
enum Option[T] { T Some  None }
fn main() i32 {
    Pair[Option[u32], Option[bool]] p = {
        .a = .Some(10),
        .b = .None
    }
    i32 acc = 0
    match p.a {
        .Some(v) { acc += (i32) v }
        .None { acc += -1 }
    }
    match p.b {
        .Some(v) { acc += (i32) v }
        .None { acc += 1 }
    }
    return acc
}
