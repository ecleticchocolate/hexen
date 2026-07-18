//@ expect val 500
// __cast crossed with a GENERIC struct target type (not just scalars):
// (Box[i32])m must dispatch through __cast[T]() with T bound to Box[i32]
// as a whole, then read back through the resulting struct's own field.
struct Box[T] { T v }
struct Meters { i32 v }
impl Meters {
    fn __cast[T]() T { return (T){.v = self.v * 100} }
}
fn main() i32 {
    Meters m = { .v = 5 }
    Box[i32] b = (Box[i32])m
    return b.v
}
