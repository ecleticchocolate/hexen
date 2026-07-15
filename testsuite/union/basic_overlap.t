//@ expect val 42
union U { i32 x  f32 y }
fn main() i32 {
    U u
    u.x = 42
    return u.x
}
