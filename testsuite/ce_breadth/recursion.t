//@ expect val 120
fn fact(u32 n) u32 { if n <= 1 { return 1 }  return n * fact(n - 1) }
const u32 X = fact(5)
fn main() i32 { return (i32) X }
