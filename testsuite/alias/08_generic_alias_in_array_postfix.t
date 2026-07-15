//@ expect val 6
alias Fn[T] = fn(T) T
fn a(u32 x) u32 { return x } fn b(u32 x) u32 { return x+1 }
fn main() i32 { (Fn[u32])[2] arr = {a, b}  return (i32)arr[1](5) }
