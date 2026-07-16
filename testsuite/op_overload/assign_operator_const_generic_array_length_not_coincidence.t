//@ expect val 0
// Same regression class as dispatch_actually_used.t: __assign here ignores
// the array contents and returns N itself (not the sum), so the test fails
// loudly if either dispatch OR the const-generic N-inference is ever wrong.
struct Wrapper { i32 v }
impl Wrapper {
    fn __assign[u32 N](i32[N] arr) void {
        self.v = (i32)N
    }
}
fn main() i32 {
    Wrapper w = { 1, 2, 3, 4, 5 }
    if (w.v != 5) { return 1 }
    return 0
}
