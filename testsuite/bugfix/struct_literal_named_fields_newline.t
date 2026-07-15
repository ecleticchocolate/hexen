//@ expect val 3
struct Vec3 { f32 x  f32 y  f32 z }
fn dot(Vec3 a, Vec3 b) f32 { return a.x*b.x + a.y*b.y + a.z*b.z }
fn main() i32 {
    Vec3 v = {
        .x=1.0
        .y=1.0
        .z=1.0
    }
    return (i32)dot(v, v)
}
