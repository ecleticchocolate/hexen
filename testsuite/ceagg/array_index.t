//@ expect val 20
fn f() u32 { u32[3] a = {10, 20, 30}  return a[1] }
const u32 X = f()
fn main() i32 { return (i32) X }
