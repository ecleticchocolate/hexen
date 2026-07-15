//@ expect err array size must be positive
struct Vec[T, u32 N] { T[N] e }
struct Trimmed[T, u32 N] { Vec[T, N - 10] v }
fn main() i32 { Trimmed[i32, 4] t  return 0 }
