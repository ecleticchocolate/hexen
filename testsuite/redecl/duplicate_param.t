//@ expect err already declared
fn f(u32 a, u32 a) u32 { return a }  fn main() i32 { return (i32) f(1, 2) }
