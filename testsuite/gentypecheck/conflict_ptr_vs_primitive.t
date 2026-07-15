//@ expect err type parameter 'T'
fn pick[T](T a, T b) T { return a }
fn main() i32 {
    u32 x = 5
    u32* p = &x
    u32 r = pick(x, p)
    return (i32) r
}
