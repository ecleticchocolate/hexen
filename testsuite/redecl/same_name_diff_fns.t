//@ expect val 9
fn a(u32 x) u32 { return x }  fn b(u32 x) u32 { return x * 2 }  fn main() i32 { return (i32)(a(3) + b(3)) }
