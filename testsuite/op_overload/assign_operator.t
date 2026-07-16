//@ expect val 10
struct Vector { i32 x  i32 y }
impl Vector {
    fn __assign(i32 scalar) void {
        self.x = scalar
        self.y = scalar
    }
}
fn main() i32 {
    Vector v = { .x = 0, .y = 0 }
    v = 5
    return v.x + v.y
}
