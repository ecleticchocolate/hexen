//@ expect val 3
fn main() i32 { i32[5] xs={1,2,10,4,5}  i32 s=0
  for i32 x in xs { if x > 5 { break } s=s+x }  return s }
