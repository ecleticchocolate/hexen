//@ expect err cannot cast to a struct with more fields
struct Point { i32 x }

fn main() i32 {
    Point p = {.x = 1}
    struct { i32 x  i32 y } p2 = (struct { i32 x  i32 y }) p
    return p2.x
}
