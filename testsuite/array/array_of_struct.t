//@ expect val 20
struct P { i32 x  i32 y }
fn main() i32 { P[2] a = {{.x = 1, .y = 2}, {.x = 10, .y = 20}}; return a[1].y }
