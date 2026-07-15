//@ expect val 110
struct V { f64 x  u32 n }  fn scale(V v) f64 { return v.x * (f64) v.n }  fn compute() f64 { V[3] vs = { {.x = 1.5, .n = 2}, {.x = 2, .n = 3}, {.x = 0.5, .n = 4} }  f64 t = 0  for u32 i = 0 to 3 { t = t + scale(vs[i]) }  return t }  const f64 R = compute()  fn main() i32 { return (i32)(R * 10.0) }
