//@ expect val -10
fn negate[T](T x) T { return 0 - x }
struct Handler { fn(i32) i32 op }
fn main() i32 { Handler h = {.op = negate}  return h.op(10) }
