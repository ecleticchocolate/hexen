//@ expect val 42
fn identity[T](T x) T { return x }
fn main() i32 { return identity(42) }
