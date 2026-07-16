//@ expect val 0
// Same regression class as dispatch_actually_used.t: __assign is deliberately
// wrong (always sets both fields to 777, ignoring the actual RHS), so the
// test fails loudly if dispatch is ever wrong.
struct Vector { i32 x  i32 y }
impl Vector {
    fn __assign(i32 scalar) void {
        self.x = 777
        self.y = 777
    }
}
fn main() i32 {
    Vector v = { .x = 0, .y = 0 }
    v = 5
    if (v.x != 777) { return 1 }
    return 0
}
