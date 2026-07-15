//@ expect val 42
fn mid[T](T a, T b, T c) T { return b }
fn main() i32 {
    u32 x = 1
    u32 y = 42
    u32 z = 3
    u32 r = mid(x, y, z)
    return (i32) r
}
