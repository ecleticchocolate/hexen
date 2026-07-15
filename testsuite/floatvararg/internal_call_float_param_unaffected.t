//@ expect val 60
fn d(f64 x) f64 { return x * 2.0 }
fn main() i32 { return (i32)(d(3) * 10.0) }
