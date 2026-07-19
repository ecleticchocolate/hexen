//@ expect val 7
// Auto-deref through a generic scrutinee: v is T = P*, unpack descends the pointer
// then projects. This was one of the 5 deref cases the naive (two-path) fix broke;
// the unified path handles it because auto-deref moved into the shared lowering.
struct P { i32 x  i32 y }
fn f[T](T v) i32 { unpack {.x = a, .y = b} = v  return a + b }
fn main() i32 {
    P p = {.x = 3, .y = 4}
    return f(&p)                   // T = P*, autoderef -> 7
}
