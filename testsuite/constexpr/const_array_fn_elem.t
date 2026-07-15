//@ expect val 20
fn double(u32 x) u32 { return x * 2 }
const u32[4] VALS = {double(10), 20, 30, 40}
fn main() i32 { return (i32) VALS[0] }
