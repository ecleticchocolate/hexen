//@ expect val 25
fn inc(u32 x) u32 { return x + 1 }
fn dbl(u32 x) u32 { return x * 2 }
fn sqr(u32 x) u32 { return x * x }
struct Fns { fn(u32) u32 f0  fn(u32) u32 f1  fn(u32) u32 f2 }
fn main() i32 {
    Fns fns = {.f0 = inc, .f1 = dbl, .f2 = sqr}
    return (i32)(fns.f0(5) + fns.f1(5) + fns.f2(3))
}
