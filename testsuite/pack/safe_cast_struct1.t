//@ expect err struct cast target must perfectly match a suffix
struct Point { i32 x  f32 y }

fn main() i32 {
    Point p = {.x = 1, .y = 2.0}
    struct { i32 y } p2 = (struct { i32 y }) p // y suffix is f32, target is i32
    return p2.y
}
