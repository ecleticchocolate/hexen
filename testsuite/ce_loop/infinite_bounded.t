//@ expect err constant expression
fn bad() u32 { u32 i = 0  while true { i = i + 1 }  return i }
const u32 X = bad()
fn main() i32 { return (i32) X }
