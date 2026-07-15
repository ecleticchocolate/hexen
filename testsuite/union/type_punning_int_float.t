//@ expect val 1065353216
union U { i32 i  f32 f }
fn main() i32 {
    U u
    u.f = 1.0
    return u.i
}
