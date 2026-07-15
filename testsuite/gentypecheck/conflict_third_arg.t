//@ expect err type parameter 'T' was inferred as 'u32' but got 'f32'
fn mid[T](T a, T b, T c) T { return b }
fn main() i32 {
    u32 x = 1
    u32 y = 2
    f32 z = 3.0
    u32 r = mid(x, y, z)
    return (i32) r
}
