//@ expect val 42
fn identity[T](T x) T { return x }
fn apply(fn(u32) u32 f, u32 x) u32 { return f(x) }
fn main() i32 { return (i32) apply(identity, 42) }
