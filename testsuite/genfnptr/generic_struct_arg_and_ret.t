//@ expect val 42
struct Box[T] { T val }
fn unbox_rebox[T](Box[T] b) Box[T] { return {.val = b.val + 1} }
fn main() i32 {
    fn(Box[i32]) Box[i32] f = unbox_rebox
    Box[i32] b = {.val = 41}
    Box[i32] r = f(b)
    return r.val
}
