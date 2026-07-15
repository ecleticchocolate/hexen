//@ expect val 42
fn inc[T](T x) T { return x + 1 }
fn double[T](T x) T { return x + x }
struct Op { fn(i32) i32 f }
fn set_op(Op* o, fn(i32) i32 f) void { o.f = f }
fn main() i32 { Op o = {.f = inc}  set_op(&o, double)  return o.f(21) }
