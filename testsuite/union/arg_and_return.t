//@ expect val 7
union U { i32 x  f32 y }
fn make(i32 v) U {
    U u
    u.x = v
    return u
}
fn main() i32 {
    U r = make(7)
    return r.x
}
