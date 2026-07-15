//@ expect err field index out of range
struct Point{i32 x i32 y i64 z}
fn main()i32{ return (i32)offsetof(Point, 5) }
