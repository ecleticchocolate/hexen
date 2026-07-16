//@ expect val 0
// Regression: __add previously never dispatched at all -- `a + b` on two
// structs silently fell through to built-in lanewise field-add no matter what
// __add said, because the dispatch check (Struct_FindField) searched DATA
// fields, and an impl method is never one. This __add is deliberately WRONG
// (swaps fields instead of adding) so the test fails loudly if dispatch ever
// regresses back to silently preferring lanewise arithmetic.
struct Vector { i32 x  i32 y }
impl Vector {
    fn __add(Vector other) Vector {
        return { .x = self.y, .y = self.x }
    }
}
fn main() i32 {
    Vector a = { .x = 1, .y = 2 }
    Vector b = { .x = 3, .y = 4 }
    Vector c = a + b
    if (c.x != 2) { return 1 }
    if (c.y != 1) { return 2 }
    return 0
}
