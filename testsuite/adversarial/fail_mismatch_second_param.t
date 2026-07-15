//@ expect err type mismatch in assignment: cannot assign Pair[u32, f64][2] to Pair[u32, u32][2]
struct Pair[U, V] { U first  V second }
fn main() i32 {
    Pair[u32, f64][2] a
    Pair[u32, u32][2] c
    c = a
    return 0
}
