//@ expect val 42
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
struct Pair[T] { fn(T) T f  fn(T) T g }
fn apply_both[T](Pair[T] p, T x) T { return p.g(p.f(x)) }
fn main() i32 { Pair[i32] p = {.f = identity, .g = double}  return apply_both(p, 21) }
