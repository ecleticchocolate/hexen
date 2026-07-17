//@ expect val 21
struct P { i32 x  i32 y }
fn main() i32 { P[3] ps={{1,2},{3,4},{5,6}}  i32 s=0  for unpack { .x=a, .y=b } in ps { s=s+a+b }  return s }
