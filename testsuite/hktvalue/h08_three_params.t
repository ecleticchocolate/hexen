//@ expect val 23
struct Mat[T, u32 R, u32 C] { T[R] a  T[C] b }
struct HKT[M] { i32 x }
fn probe[T]() i32 {
    match T {
        HKT[M] { match Mat[i32,2,3] { M[E][R][C] { return (i32)R * 10 + (i32)C } else { return 2 } } }
        else { return 3 }
    }
    return -1
}
fn main() i32 { return probe[HKT[Mat]]() }
