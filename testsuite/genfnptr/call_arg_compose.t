//@ expect val 18
fn double[T](T x) T { return x + x }
fn square[T](T x) T { return x * x }
fn compose(fn(u32) u32 f, fn(u32) u32 g, u32 x) u32 { return f(g(x)) }
fn main() i32 { return (i32) compose(double, square, 3) }
