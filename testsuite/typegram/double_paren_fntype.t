//@ expect val 42
fn inc(u32 x) u32 { return x + 1 }
fn main() i32 {
    ((fn(u32) u32))[2] a
    a[0] = inc
    return (i32)(a[0](40) + 1)
}
