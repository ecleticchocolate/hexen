//@ expect val 77
struct Point { i32 x  i32 y }
fn main() i32 { Point p = {.x = 1, .y = 2}; Point* pp = &p; pp.x = 77; return p.x }
