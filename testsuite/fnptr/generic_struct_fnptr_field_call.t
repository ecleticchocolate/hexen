//@ expect val 42
struct Handler[T] { fn(T) T f }
fn negate(i32 x) i32 { return -x }
fn main() i32 {
    Handler[i32] h = {.f = negate}
    return h.f(-42)
}
