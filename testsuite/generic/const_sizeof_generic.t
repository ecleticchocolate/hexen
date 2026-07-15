//@ expect val 8
struct Pair[T, U] { T a  U b }
const u32 SZ = sizeof(Pair[u32, bool])
fn main() i32 { return (i32) SZ }
