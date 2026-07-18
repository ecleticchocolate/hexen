//@ expect val 100
enum Option[T] { T Some  None }
fn unwrap_or[T](Option[T] o, T def) T {
    match o {
        .Some(v) { return v }
        .None { return def }
    }
    return def
}
fn main() i32 {
    Option[u32] a = .Some(99)
    Option[u32] b = .None
    return (i32) unwrap_or(a, 0) + (i32) unwrap_or(b, 1)
}
