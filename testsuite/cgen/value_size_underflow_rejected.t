//@ expect err array size must be positive
struct A[T, u32 N] { T[N - 1] d }
fn main() i32 { A[i32, 0] a  return (i32)sizeof(a) }
