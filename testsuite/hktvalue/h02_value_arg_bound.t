//@ expect val 30
struct Vec[T, u32 N] { T[N] e }
struct HKT[M] { i32 x }
fn probe[T]() i32 {
    match T { HKT[M] { match Vec[i32, 30] { M[E][N] { return (i32)N } else { return 2 } } } else { return 3 } }
    return -1
}
fn main() i32 { return probe[HKT[Vec]]() }
