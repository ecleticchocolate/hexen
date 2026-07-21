//@ expect val 4
struct Box[T] { T v }
struct HKT[M, X] { i32 x }
fn probe[T]() i32 {
    match T {
        HKT[M, U] { match Box[i32] { M[U] { U t = 0  return (i32)sizeof(t) } else { return 2 } } }
        else { return 3 }
    }
    return -1
}
fn main() i32 { return probe[HKT[Box, i32]]() }
