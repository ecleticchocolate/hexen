//@ expect val 5
// The count is a RUNTIME value, not a literal -- the cookie is what makes that
// recoverable at the delete site.
i32 g_dels = 0
struct R { i32 id }
impl R { fn __delete() void { g_dels = g_dels + 1 } }
fn main() i32 {
    u32 n = 5
    R* buf = new[n] R
    delete[] buf
    return g_dels
}
