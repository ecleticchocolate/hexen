//@ expect val 42
struct Pair[T, U] { T a  U b }
fn main() i32 {
    ((Pair[u32, u32])) p = {.a = 40, .b = 2}
    return (i32)(p.a + p.b)
}
