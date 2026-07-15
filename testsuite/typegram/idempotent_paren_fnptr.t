//@ expect val 42
fn inc(u32 x) u32 { return x + 1 }
fn main() i32 {
    (((fn(u32) u32))) f = inc
    return (i32)(f(41))
}
