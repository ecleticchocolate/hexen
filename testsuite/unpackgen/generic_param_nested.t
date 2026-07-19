//@ expect val 6
// Nested destructure of a generic-param-typed value, deferred + re-lowered.
struct In { i32 a  i32 b }
struct Out { In i  i32 c }
fn f[T](T v) i32 { unpack {{a, b}, c} = v  return a + b + c }
fn main() i32 { Out o = {.i = {1, 2}, .c = 3}  return f(o) }
