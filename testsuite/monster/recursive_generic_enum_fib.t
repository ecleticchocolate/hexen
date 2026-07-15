//@ expect val 55
enum Option[T] { T Some  None }
fn fib(u32 n) Option[u32] {
    if n < 2 { return .Some{n} }
    match fib(n - 1) {
        .Some{a} {
            match fib(n - 2) {
                .Some{b} { return .Some{a + b} }
                .None { return .None }
            }
        }
        .None { return .None }
    }
    return .None
}
fn main() i32 {
    match fib(10) { .Some{v} { return (i32) v }  .None { return -1 } }
    return -2
}
