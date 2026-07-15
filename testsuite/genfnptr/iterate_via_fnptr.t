//@ expect val 32
fn double[T](T x) T { return x + x }
fn apply_n(fn(i32) i32 f, i32 x, i32 n) i32 {
    for i32 i = 0 to n { x = f(x) }
    return x
}
fn main() i32 { return apply_n(double, 1, 5) }
