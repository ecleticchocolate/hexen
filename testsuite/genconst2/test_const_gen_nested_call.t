//@ expect stdout
//@ | sizeof poly: 40
extern fn printf(u8* fmt, ...) i32

struct Point { u32 x  u32 y }
fn Point_get_x(Point* p) u32 { return p.x }
struct Shape { Point[3] points }

struct Polygon[Shape S] {
    u32[S.points[2].get_x()] arr
}

fn main() i32 {
    Polygon[{.points = {{.x=1, .y=1}, {.x=2, .y=2}, {.x=10, .y=10}}}] poly
    const u32 size = sizeof(poly)
    printf("sizeof poly: %d\n", size)
    return 0
}
