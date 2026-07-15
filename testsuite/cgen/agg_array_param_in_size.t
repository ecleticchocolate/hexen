//@ expect val 48
struct Buf[T, u32[2] Shape] { T[Shape[0] * Shape[1]] data }
fn main() i32 {
    Buf[i32, {3, 4}] b
    return (i32)sizeof(b)   // i32[12] = 48
}
