//@ expect err type parameter 'T' was inferred as 'i32' but got 'u32'
fn pick[T](T a, T b) T { return a }
fn main() i32 {
    i32 a = -1
    u32 b = 1
    i32 r = pick(a, b)
    return r
}
