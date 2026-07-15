//@ expect val 63
fn fib(u32 n) u32 { if n < 2 { return n }  return fib(n - 1) + fib(n - 2) }
struct FibBuf[T, u32 N] { T[fib(N)] data }
impl FibBuf[T, u32 N] { fn cap() u32 { return fib(N) } }
fn main() i32 {
    FibBuf[i32, 8] f
    f.data[20] = 42
    return f.data[20] + (i32) f.cap()
}
