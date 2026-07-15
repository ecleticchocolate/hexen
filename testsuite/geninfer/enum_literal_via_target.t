//@ expect val 5
enum Option[T] { T Some  None }
fn id[T](T v) T { return v }
fn main() i32 {
    Option[u32] o = id(.Some{5})
    match o { .Some{v} { return (i32) v }  .None { return -1 } }
    return -2
}
