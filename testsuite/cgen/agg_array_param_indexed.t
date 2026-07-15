//@ expect val 100
struct Table[T, u32[4] W] { T v }
impl Table[T, u32[4] W] { fn total() u32 { return W[0]+W[1]+W[2]+W[3] } }
fn main() i32 {
    Table[i32, {10, 20, 30, 40}] t = {.v = 0}
    return (i32)t.total()   // 100
}
