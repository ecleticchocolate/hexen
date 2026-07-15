//@ expect val 2015
struct Point { i32 x  i32 y }
i32 call_count = 0
Point p1
Point p2
fn base() Point* {
    call_count = call_count + 1
    if call_count == 1 { return &p1 }
    return &p2
}
fn main() i32 {
    p1.x = 10
    p2.x = 1000
    base().x += 5
    return p1.x + p2.x + call_count * 1000
}
