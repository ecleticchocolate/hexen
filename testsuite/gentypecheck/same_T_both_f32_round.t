//@ expect val 3
fn pick[T](T a, T b) T { return b }
fn main() i32 {
    f32 a = 1.0
    f32 b = 3.0
    f32 r = pick(a, b)
    return (i32) r
}
