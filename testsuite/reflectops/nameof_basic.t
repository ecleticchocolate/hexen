//@ expect val 121
struct Point{i32 x i32 y i64 z}
fn main()i32{ u8* n = nameof(Point, 1) return (i32)n[0] }
