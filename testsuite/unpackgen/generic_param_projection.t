//@ expect val 7
// unpack {..} destructuring a value whose type is a still-generic param T.
// The concrete shape isn't known until instantiation, so this must defer -- it
// used to fail "indexing a non-array" (parse-time shape guess). Now unpack lowers
// to a single deferred match arm, re-lowered per instantiation. Same fn is used
// on a struct and an array below to prove it re-lowers, not hardcodes one shape.
struct P { i32 x  i32 y }
fn sum2[T](T v) i32 { unpack {a, b} = v  return a + b }
fn main() i32 {
    P p = {.x = 3, .y = 4}
    i32[2] q = {10, -10}
    return sum2(p) + sum2(q)   // 7 + 0 = 7
}
