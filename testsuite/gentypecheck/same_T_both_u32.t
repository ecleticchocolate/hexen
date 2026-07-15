//@ expect val 42
fn pick[T](T a, T b) T { return b }
fn main() i32 {
    u32 x = 5
    u32 y = 42
    u32 r = pick(x, y)
    return (i32) r
}
