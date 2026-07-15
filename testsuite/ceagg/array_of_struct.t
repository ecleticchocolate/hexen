//@ expect val 42
struct P { u32 x }
fn f() u32 { P[2] a = {{.x = 10}, {.x = 32}}  return a[0].x + a[1].x }
const u32 X = f()
fn main() i32 { return (i32) X }
