//@ expect val 55
struct Acc { u32 a  u32 b }
fn fib_step(Acc acc, u32 n) Acc {
    if n == 0 { return acc }
    Acc next = {.a = acc.b, .b = acc.a + acc.b}
    return fib_step(next, n - 1)
}
fn fib(u32 n) u32 {
    Acc start = {.a = 0, .b = 1}
    Acc final = fib_step(start, n)
    return final.a
}
const u32 FIB10 = fib(10)
fn main() i32 { return (i32) FIB10 }
