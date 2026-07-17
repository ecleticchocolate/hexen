//@ expect val 21
fn main() i32 { i32[2] a={1,2}  i32[2] b={3,4}  i32 s=0
  for i32 x in a { for i32 y in b { s=s+x*y } }  return s }
