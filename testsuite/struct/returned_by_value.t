//@ expect val 30
struct Point { i32 x  i32 y }
fn make(i32 a, i32 b) Point { return {.x = a, .y = b} }
fn main() i32 { Point p = make(10, 20); return p.x + p.y }
