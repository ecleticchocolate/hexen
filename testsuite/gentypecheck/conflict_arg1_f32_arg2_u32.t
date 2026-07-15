//@ expect err type parameter 'T' was inferred as 'f32' but got 'u32'
fn pick[T](T a, T b) T { return a }
fn main() i32 {
    f32 y = 3.0
    u32 x = 5
    f32 r = pick(y, x)
    return 0
}
