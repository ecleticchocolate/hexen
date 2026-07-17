//@ expect val 7
struct P { i32 x  i32 y }
fn main() i32 { P[2] ps={{1,2},{3,4}}  i32[2] outs={0,0}  u32 k=0
  for unpack { .x=outs[0], .y=outs[1] } in ps { k=k }
  return outs[0]+outs[1] }
