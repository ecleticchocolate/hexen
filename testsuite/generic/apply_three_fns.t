//@ expect val 20
fn inc(u32 x) u32 { return x + 1 }
fn dbl(u32 x) u32 { return x * 2 }
fn neg(u32 x) u32 { return x - 1 }
fn apply(fn(u32) u32 f, u32 x) u32 { return f(x) }
fn main() i32 {
    return (i32)(apply(inc, 5) + apply(dbl, 5) + apply(neg, 5))
}
