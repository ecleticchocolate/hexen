//@ expect val 0
// A struct with NO impl block must still get built-in lanewise field-add --
// the operator-method dispatch check must fall back cleanly, not just avoid
// crashing, when there's no __add to find.
struct Vector { i32 x  i32 y }
fn main() i32 {
    Vector a = { .x = 1, .y = 2 }
    Vector b = { .x = 3, .y = 4 }
    Vector c = a + b
    if (c.x != 4) { return 1 }
    if (c.y != 6) { return 2 }
    return 0
}
