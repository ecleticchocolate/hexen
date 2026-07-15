//@ expect val 4
struct Point{i32 x i32 y}
struct Box[T]{ T val }
impl Box[T] { fn f0() u64 { return offsetof(T, 1) } }
fn main() i32 { Box[Point] b return (i32)b.f0() }
