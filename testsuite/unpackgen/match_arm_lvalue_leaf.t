//@ expect val 12
struct P { i32 x  i32 y }
fn main() i32 { P p={3,9}  i32[2] out={0,0}
  match p { { .x=out[0], .y=out[1] } { return out[0]+out[1] } else { return -1 } } }
