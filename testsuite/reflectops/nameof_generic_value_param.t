//@ expect val 122
struct Point{i32 x i32 y i64 z}
struct Idx[u32 N]{ i32 dummy }
impl Idx[u32 N] { fn nm() u8* { return nameof(Point, N) } }
fn main() i32 { Idx[2] i u8* n = i.nm() return (i32)n[0] }
