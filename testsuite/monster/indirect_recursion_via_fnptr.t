//@ expect val 55
fn step(fn(u32) u32 self, u32 n) u32 {
    if n == 0 { return 0 }
    return n + self(n - 1)
}
fn rec(u32 n) u32 { return step(rec, n) }
fn main() i32 { return (i32) rec(10) }
