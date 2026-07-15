//@ expect val -7
fn pick[T](T a, T b) T { return a }
fn main() i32 {
    i32 x = -7
    i32 y = 99
    return pick(x, y)
}
