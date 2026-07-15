//@ expect val 12
struct Point { i32 x  i32 y }
fn main() i32 { Point p = {.x = 1, .y = 2}; p.x = 10; return p.x + p.y }
