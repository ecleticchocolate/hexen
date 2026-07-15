//@ expect val 42
struct P { u32 a  u32 b }
P g = {.a = 40, .b = 2}
fn main() i32 { return (i32)(g.a + g.b) }
