//@ expect val 42
enum Option[T] { T Some  None }
struct Slots { Option[u32][3] data }
fn main() i32 {
    Slots s = {.data = {.Some{10}, .None, .Some{32}}}
    i32 acc = 0
    for u32 i = 0 to 3 {
        match s.data[i] {
            .Some{v} { acc += (i32) v }
            .None { }
        }
    }
    return acc
}
