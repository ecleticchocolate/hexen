//@ expect val 40
fn a[T](T x) T { return x * 2 }
fn b(u32 x) u32 { return x }
fn main() i32 { return (i32)b(a(20)) }
