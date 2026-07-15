//@ expect val 42
struct I { u32 v }
struct O { I i  u32 k }
fn f() u32 { O o = {.i = {.v = 5}, .k = 37}  return o.i.v + o.k }
const u32 X = f()
fn main() i32 { return (i32) X }
