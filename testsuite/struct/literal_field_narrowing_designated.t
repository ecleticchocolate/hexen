//@ expect err float literal in struct field requires a float destination
struct P { i32 x  i32 y }
fn main() i32 { P p = {.x = 1.5, .y = 2}  return p.x }
