//@ expect val 60
fn f() u32 { u32[3] a = {10, 20, 30}  u32 s = 0  for u32 i = 0 to 3 { s = s + a[i] }  return s }
const u32 X = f()
fn main() i32 { return (i32) X }
