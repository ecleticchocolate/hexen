//@ expect val 42
fn id[T](T x) T { return x }
fn main() i32 { return id(42) }
