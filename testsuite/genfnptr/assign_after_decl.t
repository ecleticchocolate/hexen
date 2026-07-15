//@ expect val 42
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
fn main() i32 {
    fn(u32) u32 f = identity
    f = double
    return (i32) f(21)
}
