//@ expect val 12
struct P { i32 x  i32 y }
fn main() i32 { P p={3,9}  i32 got=0
  match p { { .x=got, .y=b } { return got+b } else { return -1 } } }
