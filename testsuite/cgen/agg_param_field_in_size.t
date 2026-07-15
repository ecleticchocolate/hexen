//@ expect val 23
struct Dim { u32 n }
struct Buf[T, Dim D] { T[D.n] data }
fn main() i32 {
    Buf[i32, {.n=4}] b
    b.data[3] = 7
    return b.data[3] + (i32)sizeof(b)   // 7 + 16 = 23
}
