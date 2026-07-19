//@ expect val 37
// THE proof it re-lowers per instantiation instead of committing to one shape:
// the SAME generic unpack fn is instantiated once for a struct and once for an
// array. A hardcoded/parse-time-shaped lowering gets one of them wrong.
struct P { i32 x  i32 y }
fn f[T](T v) i32 { unpack {a, b} = v  return a + b }
fn main() i32 {
    P p = {.x = 3, .y = 4}          // struct instantiation -> 7
    i32[2] q = {10, 20}            // array instantiation  -> 30
    return f(p) + f(q)             // 37
}
