//@ expect val 15
struct V { f32 x }
V g = {.x = 1.5}
fn main() i32 { return (i32)(g.x * 10.0) }
