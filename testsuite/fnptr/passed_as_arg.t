//@ expect val 42
fn apply(fn(i32) i32 f, i32 x) i32 { return f(x) }
fn double(i32 x) i32 { return x * 2 }
fn main() i32 { return apply(double, 21) }
