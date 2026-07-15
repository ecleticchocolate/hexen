//@ expect val 42
fn double[T](T x) T { return x + x }
struct Wrapper[T] { fn(T) T func  T val }
fn main() i32 {
    Wrapper[u32] w = {.func = double, .val = 21}
    return (i32) w.func(w.val)
}
