//@ expect val 0
// Same regression class as dispatch_actually_used.t, for __eq: two structs
// with genuinely different fields must compare equal here, proving __eq (not
// a byte-for-byte lanewise comparison) is what actually ran. Uses == directly
// (not !=) since __neq is a separate dunder this compiler does not derive
// from __eq automatically -- a `!=` test would exercise lanewise fallback
// instead of __eq dispatch.
struct Vector { i32 x  i32 y }
impl Vector {
    fn __eq(Vector other) bool { return true }
}
fn main() i32 {
    Vector a = { .x = 1, .y = 2 }
    Vector b = { .x = 999, .y = 999 }
    if (!(a == b)) { return 1 }
    return 0
}
