//@ expect val 10
struct Pair[T, U] { T a  U b }
fn sum_pairs[T](Pair[T, T] x, Pair[T, T] y) T {
    return x.a + x.b + y.a + y.b
}
fn main() i32 {
    Pair[u32, u32] p1 = {.a = 1, .b = 2}
    Pair[u32, u32] p2 = {.a = 3, .b = 4}
    return (i32) sum_pairs(p1, p2)
}
