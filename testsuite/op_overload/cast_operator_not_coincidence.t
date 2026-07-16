//@ expect val 0
// Same regression class as dispatch_actually_used.t: __cast deliberately
// ignores self and returns a fixed constant, so the test fails loudly if
// dispatch is ever wrong.
struct Meters { i32 v }
impl Meters {
    fn __cast() f64 { return 777.0 }
}
fn main() i32 {
    Meters m = { .v = 5 }
    f64 x = (f64)m
    if ((i32)x != 777) { return 1 }
    return 0
}
