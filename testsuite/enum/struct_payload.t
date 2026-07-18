//@ expect val 7
struct Point { i32 x  i32 y }
enum Shape { Point Dot  u32 Circle }
fn main() i32 {
    Shape s = .Dot( {.x = 3, .y = 4} )
    match s {
        .Dot(p) { return p.x + p.y }
        .Circle(r) { return (i32) r }
    }
    return -1
}
