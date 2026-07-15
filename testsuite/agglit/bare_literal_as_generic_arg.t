//@ expect val 7
struct Point { i32 x  i32 y }
fn get_x[T](T v) i32 { return v.x }
fn main() i32 {
    Point p = {.x = 7, .y = 3}
    return get_x(p)
}
