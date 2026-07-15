//@ expect val 111
fn collatz(u64 n) u32 {
    u32 steps = 0
    while n > 1 {
        if n % 2 == 0 { n = n / 2 } else { n = 3 * n + 1 }
        steps += 1
    }
    return steps
}
const u32 C27 = collatz(27)
fn main() i32 { return (i32) C27 }
