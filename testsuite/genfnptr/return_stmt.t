//@ expect val 30
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
fn get_fn(bool pick) fn(u32) u32 {
    if pick { return identity }
    return double
}
fn main() i32 {
    fn(u32) u32 f = get_fn(true)
    fn(u32) u32 g = get_fn(false)
    return (i32)(f(10) + g(10))
}
