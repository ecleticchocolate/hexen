//@ expect val 20100
fn sum(u32 n) u32 {
    if n == 0 { return 0 }
    return n + sum(n - 1)
}
const u32 X = sum(200)
fn main() i32 { return (i32) X }
