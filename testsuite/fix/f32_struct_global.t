//@ expect val 7
struct V { f32 x  f32 y }
V g = {.x = 3.0, .y = 4.0}
fn main() i32 { return (i32)(g.x + g.y) }
