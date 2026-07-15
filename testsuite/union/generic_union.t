//@ expect val 5
union U[T] { T val  i64 raw }
fn main() i32 {
    U[i32] u
    u.val = 5
    return u.val
}
