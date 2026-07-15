//@ expect val 7
struct Vec2 { f32 x  f32 y }
enum Shape { Vec2 Point  u32 Circle }
fn main() i32 {
    Shape s = .Point{ {.x = 3.0, .y = 4.0} }
    match s {
        .Point{v} { return (i32) v.x + (i32) v.y }
        .Circle{r} { return -1 }
    }
    return -2
}
