//@ expect err float literal in array element requires a float destination
fn main() i32 { i32[2] a = {1.5, 2}  return a[0] }
