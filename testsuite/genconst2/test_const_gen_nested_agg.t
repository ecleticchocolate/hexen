//@ expect stdout
//@ | sizeof poly: 40
extern fn printf(u8* fmt, ...) i32

struct Point { u32 x  u32 y }
struct Shape { Point[3] points }

struct Polygon[Shape S] {
    // Array size determined by deeply nesting into a const generic aggregate's array field!
    u32[S.points[2].x] arr
}

fn main() i32 {
    Polygon[{.points = {{.x=1, .y=1}, {.x=2, .y=2}, {.x=10, .y=10}}}] poly
    printf("sizeof poly: %d\n", sizeof(poly)) // Should be 40 (10 * 4)
    return 0
}
