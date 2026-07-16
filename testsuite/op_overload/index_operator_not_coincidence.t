//@ expect val 0
// Same regression class as dispatch_actually_used.t: __index deliberately
// ignores the index and self, so the test fails loudly if dispatch is ever
// wrong (there is no field-access fallback for AST_INDEX to coincidentally
// agree with, but this locks in the actual dispatched value regardless).
struct Vec { i32 x  i32 y }
impl Vec {
    fn __index(i32 i) i32 { return 777 }
}
fn main() i32 {
    Vec v = { .x = 5, .y = 9 }
    if (v[0] != 777) { return 1 }
    return 0
}
