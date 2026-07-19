//@ expect val 42
// A generic fn that unpacks, evaluated at COMPILE TIME via a const initializer.
// unpack now rides the same match-lowering as everything else, at typecheck AND
// in ConstEval -- so its binders survive comptime folding (they live in the
// transparent block, not a scoped arm if-block).
struct P { i32 x  i32 y }
fn ext[T](T v) i32 { unpack {a, b} = v  return a + b }
const P PP = {.x = 30, .y = 12}
const i32 K = ext(PP)
fn main() i32 { return K }
