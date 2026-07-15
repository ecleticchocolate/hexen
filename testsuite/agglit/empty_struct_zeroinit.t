//@ expect val 0
struct P { i32 x  i32 y }
fn main() i32 { P p = {} return p.x + p.y }
