//@ expect val 33
struct Vec { i32 x  i32 y }
impl Vec {
    fn add(i32 dx, i32 dy) { self.x = self.x + dx  self.y = self.y + dy }
    fn sum() i32 { return self.x + self.y }
}
fn main() i32 {
    Vec v = {.x = 1, .y = 2}
    v.add(10, 20)
    return v.sum()
}
