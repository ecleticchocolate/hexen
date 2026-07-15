//@ expect val 5
struct W[T, u32 N] { T[N] d }
fn mk[T, u32 N]() W[T, N] {
    W[T, N] w  u32 i = 0
    while i < N { w.d[i] = (T)(i + 1)  i = i + 1 }
    return w
}
fn take(W[i32, 5] w) i32 { return w.d[4] }
fn main() i32 { return take(mk()) }
