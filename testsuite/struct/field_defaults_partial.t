//@ expect val 5
struct Point { i32 x = 0  i32 y = 0 }
fn main() i32 {
    Point p = {.x = 5}
    return p.x + p.y
}
