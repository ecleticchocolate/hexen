//@ expect val 42
fn inc[T](T x) T { return x + 1 }
fn double[T](T x) T { return x + x }
fn compose(fn(i32) i32 f, fn(i32) i32 g, i32 x) i32 { return f(g(x)) }
fn main() i32 { return compose(double, inc, 20) }
