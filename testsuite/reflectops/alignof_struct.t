//@ expect val 8
struct Point{i32 x i32 y i64 z}
fn main()i32{ return (i32)alignof(Point) }
