//@ expect val 4
struct Vec3 { f32 x = 0.0  f32 y = 0.0  f32 z = 1.0 }
fn main() i32 { Vec3 v = {.x = 3.0} return (i32)(v.x + v.y + v.z) }
