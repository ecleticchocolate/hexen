//@ expect val 7
struct Point { i32 x  i32 y }
fn main() i32 { Point p = {.x = 3, .y = 4}; return p.x + p.y }
