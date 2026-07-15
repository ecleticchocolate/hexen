//@ expect val 0
struct Point{f32 x f32 y}
fn a[T](u32 seed) T { return (T){} }
fn b(Point p) i32 { return (i32)p.x }
fn main() i32 { return b(a(20)) }
