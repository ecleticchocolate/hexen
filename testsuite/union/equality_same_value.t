//@ expect val 1
union U { i32 x }
fn main() i32 {
    U a
    U b
    a.x = 1
    b.x = 1
    if a == b { return 1 }
    return 0
}
