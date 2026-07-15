//@ expect val 11
struct Pair[T, U] { T a  U b }
fn main() i32 {
    Pair[u32, bool] p = {.a = 10, .b = true}
    return (i32) p.a + (i32) p.b
}
