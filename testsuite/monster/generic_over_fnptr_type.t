//@ expect val 42
fn call_it[T](T f, u32 x) u32 { return f(x) }
fn tri(u32 x) u32 { return x * 3 }
fn main() i32 { return (i32) call_it(tri, 14) }
