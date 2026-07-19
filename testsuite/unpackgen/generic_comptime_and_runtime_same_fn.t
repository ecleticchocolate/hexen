//@ expect val 84
// The SAME generic unpack fn used at COMPILE time (const) AND at runtime -- the
// four-cases-into-one collapse in a single test. Comptime binders must survive
// folding (transparent block); runtime must still destructure. Both = 42.
struct P { i32 x  i32 y }
fn ext[T](T v) i32 { unpack {a, b} = v  return a + b }
const P PP = {.x = 30, .y = 12}
const i32 K = ext(PP)              // comptime -> 42
fn main() i32 {
    P rp = {.x = 30, .y = 12}
    return K + ext(rp)             // 42 (comptime) + 42 (runtime) = 84
}
