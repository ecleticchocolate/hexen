//@ expect val 42
fn identity[T](T x) T { return x }
fn main() i32 {
    fn(i32)  i32  f = identity
    fn(u32)  u32  g = identity
    fn(bool) bool h = identity
    return (i32)(f(20) + (i32)g(21) + (i32)h(true))
}
