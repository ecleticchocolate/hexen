//@ expect val 5
struct Point{f32 x f32 y}
fn make[T](u32 seed) T { return (T){} }
fn combine(i32 tag, Point p) i32 { return tag + (i32)p.x }
fn main() i32 { return combine(5, make(1)) }
