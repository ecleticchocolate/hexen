//@ expect err unpack pattern must always match
struct Point { i32 x  i32 y }
fn main() i32 {
    Point p = { .x = 3, .y = 9 }
    unpack { .x = 3, .y = py } = p
    return py
}
