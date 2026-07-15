//@ expect val 9
fn s() u32 { u32 a = 0  for u32 i = 0 to 3 { for u32 j = 0 to 3 { a = a + 1 } }  return a }
const u32 X = s()
fn main() i32 { return (i32) X }
