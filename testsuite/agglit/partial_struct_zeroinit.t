//@ expect val 7
struct P { i32 x  i32 y  i32 z }
fn main() i32 { P p = {7} return p.x + p.y + p.z }
