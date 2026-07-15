//@ expect val 42
struct P { u32 x  u32 y }
fn main() i32 { P p = {40, 2}  return (i32)(p.x + p.y) }
