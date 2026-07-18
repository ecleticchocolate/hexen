//@ expect val 40
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32][3] arr = {
        .Some(10),
        .None,
        .Some(30)
    }
    i32 acc = 0
    for u32 i = 0 to 3 {
        match arr[i] {
            .Some(v) { acc += (i32) v }
            .None { }
        }
    }
    return acc
}
