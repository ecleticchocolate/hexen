//@ expect val 9
struct Vec3 { f32 x  f32 y  f32 z }
fn main() i32 {
    Vec3 v = {.x=2.0, .y=3.0, .z=4.0}
    return (i32)(v.x + v.y + v.z)
}
