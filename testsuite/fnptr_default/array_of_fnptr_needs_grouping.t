//@ expect val 20
fn op_add(i32 a, i32 b) i32 { return a + b }
fn op_sub(i32 a, i32 b) i32 { return a - b }
struct OpTable[u32 N] { (fn(i32, i32) i32)[N] slots }
impl OpTable[u32 N] { fn call(u32 idx, i32 a, i32 b) i32 { return self.slots[idx](a, b) } }
fn main() i32 {
    OpTable[2] t
    t.slots[0] = op_add
    t.slots[1] = op_sub
    return t.call(0, 10, 3) + t.call(1, 10, 3)
}
