//@ expect val 3
// Regression guard: a struct declaring __assign(i32) must NOT hijack ordinary
// same-type struct assignment (b = a) just because the method NAME exists --
// Method_Resolve alone can't tell "b = a" (Vector = Vector) from "v = 5"
// (Vector = i32); the argument must actually fit __assign's declared
// parameter type, or plain assignment must still run.
struct Vector { i32 x  i32 y }
impl Vector {
    fn __assign(i32 scalar) void {
        self.x = scalar
        self.y = scalar
    }
}
fn main() i32 {
    Vector a = { .x = 1, .y = 2 }
    Vector b = { .x = 0, .y = 0 }
    b = a
    return b.x + b.y
}
