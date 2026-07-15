//@ expect val 42
struct Point { i32 x  i32 y }
fn sum_point[T](T p) i32 { return p.x + p.y }
fn main() i32 { fn(Point) i32 f = sum_point  Point p = {.x = 20, .y = 22}  return f(p) }
