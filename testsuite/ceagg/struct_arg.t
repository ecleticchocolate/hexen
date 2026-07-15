//@ expect val 42
struct P { u32 a  u32 b }
fn sum(P p) u32 { return p.a + p.b }
fn f() u32 { P p = {.a = 40, .b = 2}  return sum(p) }
const u32 X = f()
fn main() i32 { return (i32) X }
