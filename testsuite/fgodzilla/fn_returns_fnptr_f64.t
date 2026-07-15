//@ expect val 100
fn make_scaler() fn(f64) f64 { return scale }  fn scale(f64 x) f64 { return x * 2.5 }  fn main() i32 { fn(f64) f64 f = make_scaler()  return (i32)(f(4.0) * 10.0) }
