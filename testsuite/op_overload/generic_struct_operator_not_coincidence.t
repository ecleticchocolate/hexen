//@ expect val 3
// Same regression class as dispatch_actually_used.t, for a generic struct:
// __add here deliberately ignores `other` (returns self.v unchanged), so the
// test fails loudly if dispatch is ever wrong.
struct Box[T] { T v }
impl Box[T] {
    fn __add(Box[T] other) Box[T] {
        return { .v = self.v }
    }
}
fn main() i32 {
    Box[i32] a = { .v = 3 }
    Box[i32] b = { .v = 4 }
    Box[i32] c = a + b
    return c.v
}
