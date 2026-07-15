//@ expect val 7
struct Point { i32 x  i32 y }
fn main() i32 {
    Point* p = new[1] Point
    p.x = 3
    p.y = 4
    i32 r = p.x + p.y
    delete p
    return r
}
