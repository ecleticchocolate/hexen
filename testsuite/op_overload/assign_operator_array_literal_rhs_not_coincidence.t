//@ expect val 0
// Same regression class as dispatch_actually_used.t: __assign here
// deliberately ignores the array contents (always sets self.v = 777), so
// the test fails loudly if dispatch on a brace-literal RHS is ever wrong.
struct Wrapper { i32 v }
impl Wrapper {
    fn __assign(i32[3] arr) void {
        self.v = 777
    }
}
fn main() i32 {
    Wrapper w = { .v = 0 }
    w = { 1, 2, 3 }
    if (w.v != 777) { return 1 }
    return 0
}
