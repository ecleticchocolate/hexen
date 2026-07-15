//@ expect val 10
struct Pair[T, U] { T a  U b }
fn main() i32 {
    Pair[u32, bool] p = {.a = 9, .b = true}
    Pair[u32, bool]* pp = &p
    return (i32) pp.a + (i32) pp.b
}
