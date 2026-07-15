//@ expect val 8
struct Point{i32 x i32 y i64 z}
struct Idx[u32 N]{ i32 dummy }
impl Idx[u32 N] { fn off() u64 { return offsetof(Point, N) } }
fn main() i32 { Idx[2] i return (i32)i.off() }
