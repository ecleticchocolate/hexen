//@ expect val 2
struct Vec[T, u32 N] { T[N] e }
struct HKT[M] { i32 x }
fn probe[T]() i32 {
    match T { HKT[M] { match Vec[i32, 30] { M[i32][99] { return 1 } else { return 2 } } } else { return 3 } }
    return -1
}
fn main() i32 { return probe[HKT[Vec]]() }
