//@ expect val 42
union V { i32 i  f32 f }
fn main() i32 { V v  v.i=42  unpack { .i=a, .f=b } = v  return a }
