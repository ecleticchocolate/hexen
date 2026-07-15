//@ expect val 42
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
struct Ops { fn(u32) u32 f  fn(i32) i32 g }
fn main() i32 {
    Ops o = {.f = double, .g = identity}
    return (i32)(o.f(10) + (u32)o.g(22))
}
