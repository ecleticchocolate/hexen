//@ expect err cannot cast a non-struct to a struct
struct Point { i32 x  i32 y }

fn main() i32 {
    u64 val = 0
    Point p = (Point) val
    return p.x
}
