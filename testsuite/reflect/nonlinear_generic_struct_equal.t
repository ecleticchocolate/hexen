//@ expect val 10
struct Pair[A,B] { A f  B s }
fn main() i32 {
    i32 r1 = 0
    match Pair[u32, u32] { Pair[E, E] { r1 = 1 } else { r1 = 0 } }
    i32 r2 = 0
    match Pair[u32, u64] { Pair[E, E] { r2 = 1 } else { r2 = 0 } }
    return r1 * 10 + r2
}
