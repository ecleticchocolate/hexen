//@ expect val 1
fn check(u32* p) u32 {
    if p == null { return 1 }
    return 2
}
fn main() i32 { return (i32)check(null) }
