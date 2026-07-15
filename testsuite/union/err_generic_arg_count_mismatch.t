//@ expect err generic union 'U' expects 1 type arguments, got 2
union U[T] { T val  i64 raw }
fn main() i32 {
    U[i32, i32] u
    return 0
}
