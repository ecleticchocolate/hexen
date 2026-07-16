//@ expect err a type cannot be used as a value
fn main() i32 { i32[2] a = {void, 1}  return a[1] }
