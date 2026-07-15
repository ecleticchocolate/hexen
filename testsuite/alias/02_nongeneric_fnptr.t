//@ expect val 42
alias IntFn = fn(u32) u32
fn dbl(u32 x) u32 { return x*2 }
fn main() i32 { IntFn f = dbl  return (i32)f(21) }
