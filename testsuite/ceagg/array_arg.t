//@ expect val 7
fn first(u32[3] a) u32 { return a[0] }
fn f() u32 { u32[3] x = {7, 8, 9}  return first(x) }
const u32 X = f()
fn main() i32 { return (i32) X }
