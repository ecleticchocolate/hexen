//@ expect val 42
fn apply[T](fn(T) T f, T x) T { return f(x) }
fn double_u32(u32 x) u32 { return x * 2 }
fn main() i32 { return (i32) apply(double_u32, 21) }
