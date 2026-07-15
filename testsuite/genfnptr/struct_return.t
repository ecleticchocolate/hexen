//@ expect val 41
struct Pair { i32 a  i32 b }
fn make_pair[T](T x) Pair { return {.a = x, .b = x + 1} }
fn main() i32 { fn(i32) Pair f = make_pair  Pair r = f(20)  return r.a + r.b }
