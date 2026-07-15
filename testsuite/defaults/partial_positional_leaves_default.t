//@ expect val 47
struct Point { i32 x = 7  i32 y = 42 }
fn main() i32 { Point p = {5} return p.x + p.y }
