//@ expect val 4
struct A[T, u64 N] { u8[N] d }
fn f[T]() u64 { A[T, sizeof(T)] a  return sizeof(a) }
fn main() i32 { return (i32) f[u32]() }
