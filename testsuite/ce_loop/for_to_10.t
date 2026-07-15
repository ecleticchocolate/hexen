//@ expect val 45
fn s() u32 { u32 a = 0  for u32 i = 0 to 10 { a = a + i }  return a }
const u32 X = s()
fn main() i32 { return (i32) X }
