//@ expect val 6
fn main() i32 { i32[4] xs={1,2,3,4}  i32 s=0
  for i32 x in xs { if x == 4 { continue } s=s+x }  return s }
