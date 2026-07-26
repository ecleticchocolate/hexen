//@ expect val 1
// The element count expression must be evaluated exactly ONCE, at the
// allocation. It is spilled to a frame slot rather than re-evaluated, both for
// the malloc size and for the cookie.
i32 g_calls = 0
fn side() u32 { g_calls = g_calls + 1  return 3 }
struct R { i32 id }
impl R { fn __delete() void { } }
fn main() i32 {
    R* buf = new[side()] R
    delete[] buf
    return g_calls
}
