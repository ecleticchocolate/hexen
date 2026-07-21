//@ expect val 5
// Fixed params bind positionally; everything after the pack slot bundles.
// A value param before the pack still works (the pack only absorbs the surplus).
struct Tagged[u32 N, Ts...] { u32 tag  Ts rest }
fn main() i32 {
    Tagged[7, i32, u8] t
    t.tag = 5
    return (i32)t.tag
}
