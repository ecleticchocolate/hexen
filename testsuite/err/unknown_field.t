//@ expect err has no field
struct Point { i32 x  i32 y }
fn main() i32 { Point p = {.x = 1, .y = 2}; return p.z }
