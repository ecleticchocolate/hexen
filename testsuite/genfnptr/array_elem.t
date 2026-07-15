//@ expect val 40
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
fn square[T](T x) T { return x * x }
fn main() i32 {
    (fn(u32) u32)[3] arr = {identity, double, square}
    return (i32)(arr[0](5) + arr[1](5) + arr[2](5))
}
