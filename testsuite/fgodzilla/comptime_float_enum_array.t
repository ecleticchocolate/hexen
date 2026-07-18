//@ expect val 120
enum Opt[T] { T Some  None }  fn mk(f64 x) Opt[f64] { return .Some(x * 2) }  fn compute() f64 { Opt[f64][3] xs = { mk(1), mk(2), mk(3) }  f64 acc = 0  for u32 i = 0 to 3 { match xs[i] { .Some(v) { acc = acc + v }  .None { } } }  return acc }  const f64 R = compute()  fn main() i32 { return (i32)(R * 10.0) }
