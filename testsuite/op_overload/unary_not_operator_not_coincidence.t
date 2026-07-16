//@ expect val 0
// Same regression class as index_operator_not_coincidence.t: __not is
// deliberately wrong (always true, ignoring self.on's real value), so the
// test fails loudly if dispatch is ever wrong.
struct Flag { bool on }
impl Flag {
    fn __not() bool { return true }
}
fn main() i32 {
    Flag f = { .on = true }
    if (!f) { return 0 }
    return 1
}
