//@ expect val 11
enum Option[T] { T Some  None }
fn get_or_zero[T](Option[T] o) T {
    match o {
        .Some(v) { return v }
        .None { return (T) 0 }
    }
    return (T) 0
}
fn main() i32 {
    Option[u32] a = .Some(10)
    Option[u32] b = .None
    Option[bool] c = .Some(true)
    return (i32) get_or_zero(a) + (i32) get_or_zero(b) + (i32) get_or_zero(c)
}
