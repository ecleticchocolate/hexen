//@ expect val 42
fn foo[T](T v) T { auto x = v  return x }
fn main() u32 { return foo((u32)42) }
