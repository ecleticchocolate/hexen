//@ expect val 19
fn add[T](T a, T b) T { return a + b }
fn mul[T](T a, T b) T { return a * b }
struct BinOps { fn(i32,i32) i32 add  fn(i32,i32) i32 mul }
fn eval(BinOps* ops, i32 a, i32 b) i32 { return ops.add(a, b) + ops.mul(a, b) }
fn main() i32 { BinOps ops = {.add = add, .mul = mul}  return eval(&ops, 3, 4) }
