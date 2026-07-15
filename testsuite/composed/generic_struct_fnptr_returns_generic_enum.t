//@ expect val 42
enum Option[T] { T Some  None }
struct Handler[T] { fn(T) Option[T] f  T default_val }
fn maybe_double(u32 x) Option[u32] {
    if x > 10 { return .Some{x * 2} }
    return .None
}
fn run[T](Handler[T] h) T {
    Option[T] result = h.f(h.default_val)
    match result {
        .Some{v} { return v }
        .None { return h.default_val }
    }
    return h.default_val
}
fn main() i32 {
    Handler[u32] h = {.f = maybe_double, .default_val = 21}
    return (i32) run(h)
}
