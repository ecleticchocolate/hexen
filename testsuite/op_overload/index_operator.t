//@ expect val 5
struct Vec { i32 x  i32 y }
impl Vec {
    fn __index(i32 i) i32 {
        if (i == 0) { return self.x }
        return self.y
    }
}
fn main() i32 {
    Vec v = { .x = 5, .y = 9 }
    return v[0]
}
