//@ expect val 77
enum Option[T] { T Some  None }
fn main() i32 {
    u32 x = 77
    Option[u32*] o = .Some{&x}
    match o {
        .Some{p} { return (i32) *p }
        .None { return -1 }
    }
    return -2
}
