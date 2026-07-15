//@ expect val 0
union U { i32 x }
fn main() i32 {
    U a
    U b
    a.x = 1
    b.x = 2
    if a == b { return 1 }
    return 0
}
