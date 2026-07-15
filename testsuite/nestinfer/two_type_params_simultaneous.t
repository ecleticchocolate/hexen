//@ expect val 0
struct Pair[A,B]{A first B second}
fn make_pair[A,B](u32 seed) Pair[A,B] { return {} }
fn consume(Pair[i32,f32] p) i32 { return (i32)p.first + (i32)p.second }
fn main() i32 { return consume(make_pair(1)) }
