//@ expect val 120
fn f(u32 n) u32 { u32 r = 1  u32 i = 1  while i <= n { r = r * i  i = i + 1 }  return r }
const u32 X = f(5)
fn main() i32 { return (i32) X }
