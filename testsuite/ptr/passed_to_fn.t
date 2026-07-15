//@ expect val 42
fn increment(i32* p) { *p += 1 }
fn main() i32 { i32 x = 41; increment(&x); return x }
