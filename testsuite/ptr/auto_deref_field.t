//@ expect val 5
struct Point { i32 x  i32 y }
fn main() i32 { Point p = {.x = 5, .y = 3}; Point* pp = &p; return pp.x }
