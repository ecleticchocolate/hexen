//@ expect val 9
struct Vec2 { f32 x  f32 y }
impl Vec2 {
    fn dot(Vec2 other) f32 { return self.x * other.x + self.y * other.y }
    fn scale(f32 s) { self.x = self.x * s  self.y = self.y * s }
}
fn main() i32 {
    Vec2 a = {.x = 3.0, .y = 4.0}
    Vec2 b = {.x = 1.0, .y = 0.0}
    f32 d = a.dot(b)
    a.scale(2.0)
    return (i32)(d + a.x)
}
