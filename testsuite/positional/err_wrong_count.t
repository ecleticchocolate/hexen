//@ expect err expected 2 fields
struct P { u32 x  u32 y }
fn main() i32 { P p = {1, 2, 3}  return (i32) p.x }
