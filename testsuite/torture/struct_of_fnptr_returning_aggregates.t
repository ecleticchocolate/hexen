//@ expect val 10
struct Point { i32 x i32 y }
struct Generator { fn() (Point[2]) get_points }
fn make_points() Point[2] { return { {.x=1, .y=2}, {.x=3, .y=4} } }
fn main() i32 {
    Generator g = { .get_points = make_points }
    Point[2] pts = g.get_points()
    return pts[0].x + pts[0].y + pts[1].x + pts[1].y
}
