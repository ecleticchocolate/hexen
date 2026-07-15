//@ expect val 10
fn s(u32 n) u32 { u32 a = 0  u32 i = 0  while i < n { a = a + i  i = i + 1 }  return a }
const u32 X = s(5)
fn main() i32 { return (i32) X }
