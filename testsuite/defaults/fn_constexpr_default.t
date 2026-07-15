//@ expect val 60
fn fib(i32 n) i32 { if n <= 1 { return n } return fib(n-1) + fib(n-2) }
struct Task { i32 priority = fib(10)  i32 retries = 3 }
fn main() i32 {
    Task t = {}
    Task t2 = {.retries = 5}
    return t.priority + t2.retries
}
