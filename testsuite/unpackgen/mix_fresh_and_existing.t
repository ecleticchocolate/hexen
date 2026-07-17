//@ expect val 12
struct P { i32 x  i32 y }
fn main() i32 { P p={3,9}  i32 slot=0  unpack { .x=a, .y=slot } = p  return a+slot }
