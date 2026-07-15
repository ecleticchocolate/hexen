//@ expect val 1
struct Point { i32 x  i32 y }
fn main() i32 {
    Point a = {.x = 1, .y = 2}
    Point b = a
    b.x = 99
    return a.x
}
