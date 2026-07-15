//@ expect err type parameter 'T' was inferred as 'u32' but got 'f32'
fn pick[T](T a, T b) T { return a }
fn main() i32 {
    u32 x = 5
    f32 y = 3.0
    u32 r = pick(x, y)
    return (i32) r
}
