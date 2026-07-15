//@ expect val 10
fn clamp[T](T lo, T hi, T x) T { if x < lo { return lo } if x > hi { return hi } return x }
fn main() i32 { fn(i32, i32, i32) i32 f = clamp  return f(0, 10, 15) }
