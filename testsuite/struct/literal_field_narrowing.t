//@ expect err float literal in struct field requires a float destination
struct P { i32 x  i32 y }
fn main() i32 { P p = {1.5, 2}  return p.x }
