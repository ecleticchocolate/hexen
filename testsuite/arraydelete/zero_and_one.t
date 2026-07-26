//@ expect val 1
// n == 0 destroys nothing (and must not underflow the loop); n == 1 destroys once.
i32 g_dels = 0
struct R { i32 id }
impl R { fn __delete() void { g_dels = g_dels + 1 } }
fn main() i32 {
    R* z = new[0] R
    delete[] z
    R* o = new[1] R
    delete[] o
    return g_dels
}
