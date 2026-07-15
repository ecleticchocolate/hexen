//@ expect val 42
fn identity[T](T x) T { return x }
fn wrap[T](fn(T) T f, T x) T { return f(x) }
fn main() i32 { return (i32) wrap(identity, 42) }
