//@ expect err union 'U' has no field 'y'
union U { i32 x }
fn main() i32 {
    U u
    u.y = 1
    return 0
}
