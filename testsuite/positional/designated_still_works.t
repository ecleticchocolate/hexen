//@ expect val 42
struct P { u32 x  u32 y }
fn main() i32 { P p = {.y = 2, .x = 40}  return (i32)(p.x + p.y) }
