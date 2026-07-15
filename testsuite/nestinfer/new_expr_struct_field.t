//@ expect val 7
struct Point { f32 x  f32 y }
fn make_f32[T](u32 seed) T { return (T) 7.0 }
fn main() i32 {
    Point* p = new Point{.x = make_f32(1), .y = 2.0}
    i32 r = (i32)p.x
    delete p
    return r
}
