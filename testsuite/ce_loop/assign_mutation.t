//@ expect val 42
fn f() u32 { u32 x = 1  x = x + 41  return x }
const u32 X = f()
fn main() i32 { return (i32) X }
