//@ expect val 7
fn max(i32 a, i32 b) i32 { if a > b { return a } return b }
fn main() i32 { return max(3, 7) }
