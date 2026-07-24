//@ expect val 6
struct Vec3 { i32 x  i32 y  i32 z }
fn main() i32 {
    Vec3* v = new Vec3{1, 2, 3}
    return v.x + v.y + v.z
}
