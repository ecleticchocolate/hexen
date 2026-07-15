//@ expect val 42
fn identity[T](T x) T { return x }
fn main() i32 {
    fn(u32) u32 f = identity
    fn(i32) i32 g = identity
    return (i32)(f(30) + (u32)g(12))
}
