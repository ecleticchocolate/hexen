//@ expect val 77
enum Option[T] { T Some  None }
fn unwrap_ptr[T](Option[T*] o) T {
    match o {
        .Some(p) { return *p }
        .None { return (T) 0 }
    }
    return (T) 0
}
fn main() i32 {
    u32 x = 77
    Option[u32*] o = .Some(&x)
    return (i32) unwrap_ptr(o)
}
