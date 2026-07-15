//@ expect val 8
struct Pair[T, U] { T a  U b }
fn make_pair[T, U](T a, U b) Pair[T, U] { return {.a = a, .b = b} }
fn main() i32 {
    Pair[u32, bool] p = make_pair(7, true)
    return (i32) p.a + (i32) p.b
}
