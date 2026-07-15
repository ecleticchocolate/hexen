//@ expect val 10
fn swap_ret[U, V](U a, V b, bool which) U { return a }
fn main() i32 {
    u32 a = 10
    i32 b = 20
    u32 r = swap_ret(a, b, true)
    return (i32) r
}
