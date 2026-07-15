//@ expect val 7
fn op_add(i32 a, i32 b) i32 { return a + b }
struct Table { (fn(i32, i32) i32)[2] slots }
fn main() i32 { Table t  t.slots[0] = op_add  return t.slots[0](3, 4) }
