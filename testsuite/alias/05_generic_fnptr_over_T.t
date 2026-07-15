//@ expect val 42
alias Callback[T] = fn(T) T
fn inc(u32 x) u32 { return x+1 }
fn main() i32 { Callback[u32] cb = inc  return (i32)cb(41) }
