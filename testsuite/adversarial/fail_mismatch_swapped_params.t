//@ expect err type mismatch in assignment: cannot assign Pair[u32, f64][2] to Pair[f64, u32][2]
struct Pair[U, V] { U first  V second }
fn main() i32 {
    Pair[u32, f64][2] a
    Pair[f64, u32][2] b
    b = a
    return 0
}
