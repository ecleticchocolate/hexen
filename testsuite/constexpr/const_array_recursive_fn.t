//@ expect val 13
fn fib(u32 n) u32 { if n <= 1 { return n } return fib(n-1) + fib(n-2) }
const u32[4] FIBS = {fib(5), fib(6), fib(7), fib(8)}
fn main() i32 { return (i32) FIBS[2] }
