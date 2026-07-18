//@ expect val 9
// __assign boundary: a Vector already has an __assign(i32) overload, but
// assigning ANOTHER Vector (same type, already fits) must NOT dispatch
// through __assign -- it's an ordinary struct copy. If __assign wrongly
// fired here (or the "does it already fit" check was inverted), this would
// either fail to compile (no __assign(Vector) overload exists) or silently
// do the wrong thing.
struct Vector { i32 x  i32 y }
impl Vector {
    fn __assign(i32 scalar) void {
        self.x = scalar
        self.y = scalar
    }
}
fn main() i32 {
    Vector a = { .x = 4, .y = 5 }
    Vector b = { .x = 0, .y = 0 }
    b = a          // same-type assign: ordinary copy, __assign NOT invoked
    return b.x + b.y
}
