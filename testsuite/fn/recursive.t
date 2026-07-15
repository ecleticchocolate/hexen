//@ expect val 720
fn fact(u32 n) u32 { if n <= 1 { return 1 } return n * fact(n - 1) }
fn main() i32 { return (i32) fact(6) }
