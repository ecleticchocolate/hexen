//@ expect val 31
struct Inner[T, u32 N] { T[N] data }
struct Outer[T, u32 M] { Inner[T, M * 2] b }
fn main() i32 {
    Outer[i32, 3] o
    o.b.data[5] = 7
    return o.b.data[5] + (i32)sizeof(o)   // i32[6]=24 + 7 = 31
}
