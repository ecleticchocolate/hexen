//@ expect val 50
struct P { i32 x  i32 y }
fn main() i32 { P[2] ps={{10,1},{20,2}}  i32 s=0  for unpack { a, b } in ps { s=s+a*b }  return s }
